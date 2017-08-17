/**
 * DOC: V120 Interrupt Dispatcher
 *
 * A basic daemon for dispatching V120 interrupts.  Nothing too fancy, but it
 * works as both a usable interrupt dispatcher and a starting point for one
 * better tuned to a customer application.
 *
 * Sending a USR1 signal to the daemon will dump the current server status
 * information to syslog.
 *
 * The use of the interrupt dispatcher is an all-or-nothing proposition.  If
 * you're going to use it; then the dispatcher should have sole responsibility
 * for all of the interrupt handling in the system other than servicing
 * VME registers to complete RORA interrupt cycles.  It will handle enabling
 * and disabling interrupts, it will handle fetching vectors for registered
 * interrupts, the whole nine yards.  Don't step on its toes by trying to mix
 * and match.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */


#define _GNU_SOURCE 1
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"

#include <V120.h>

#include "v120irqd_intl.h"
#include "irq_vector_table.h"

#ifndef VERSION
#  error No VERSION defined.
#endif

#ifdef HAVE_MMAN_H
#  include <sys/mman.h>
#  ifdef HAVE_SCHED_H
#    include <sched.h>
#  endif
#endif

/**********************************************************************
 * Global variables
 **********************************************************************/

/* We need a file descriptor list for the main polling loop.  We'll
 * keep it in the following order:
 * 0 <= n < len_crates				VME endpoints
 * len_crates						UNIX socket accept point
 * len_crates < n < len_pollfds		Open client connections
 *
 * That's also the appending order: first VME endpoints, then the
 * socket accept, then the dynamically resizing open clients list.
 *
 * We'll also have two functions for working with the list.  In the
 * event that something stupid happens to those functions, we'll just
 * abort the program.  That includes trying to build up the non-dynamic
 * portion of the list out of order.
 */

static struct pollfd* list_pollfds = NULL;
static unsigned int len_crates = 0;
static unsigned int len_pollfds = 0;

static struct {
	bool allowFakeIrq;
	int debugMode;
	int noDaemon;
	int noVME;
} settings;

struct v120_info_t {
	V120_HANDLE * handle;
	V120_IRQ * irqhndl;
	int cratenumber;
};

/* We'll just statically allocate an array of 16 v120_info members and
 * only populate then first len_crates of them.  This wastes a trival
 * amount of memory, and saves a lot of messing around with dynamic
 * memory allocation.
 *
 * v120_info[n] corresponds to list_pollfds[n] for 0 <= n < len_crates.
 */
static struct v120_info_t v120_info[16];

/* A global variable for the signal caught by the mainloop signal handler.
 * This is set by signalhandler and cleared by mainloop.
 */
volatile sig_atomic_t caughtsignal = 0;
void signalhandler(int sig);

/**********************************************************************
 * list_pollfds management functions
 **********************************************************************/

/**
 * append_fd() - Enlarge list_pollfds and add the new fd to it.
 * @fd:		The file descriptor to add.
 * @events:	The poll events to register this fd for.
 *
 * Return: The new entry or NULL on failure.
 */
static struct pollfd* append_fd(int fd, short events)
{
	struct pollfd* newmem;
	len_pollfds++;
	newmem = realloc(list_pollfds, sizeof(*list_pollfds)*len_pollfds);
	if (newmem == NULL) {
		logcrit("realloc failed: %s", strerror(errno));
		return NULL;
	}
	list_pollfds = newmem;
	newmem = list_pollfds + (len_pollfds - 1);
	newmem->fd = fd;
	newmem->events = events;
	return newmem;
}

/**
 * remove_fd() - Remove a single fd from list_pollfds.
 *
 * Only client fds can be removed, an attempt to remove either a VME fd or the
 * server socket will cause a fatal error and abort.
 */
static void remove_fd(int idx)
{
	if (idx <= len_crates || idx >= len_pollfds) {
		logcrit("Removal index %d outside valid range %d-%d.", idx, len_crates+1, len_pollfds-1);
		exit(1);
	}

	/* Collapse the list overtop of the deleted fd. */
	memmove(list_pollfds+idx, list_pollfds+idx+1, sizeof(*list_pollfds)*(len_pollfds-idx));
	len_pollfds--;

	/* We won't bother to realloc over the missing space  This isn't
	 * a memory leak so much as simply holding the space used at the
	 * high water mark; subsequent append_fds will have a quicker time
	 * reallocing if we don't take the missing memory away.
	 */
	return;
}

/**
 * count_clients() - Count client sockets currently in list_pollfds.
 */
static unsigned int count_clients(void)
{
	return len_pollfds-len_crates-1;
}

/**
 * configureVmeEndpoints() - Add all available VME endpoints to list_pollfds.
 *
 * Must be called as the first endpoints added, i.e. with an empty list_pollfds.
 *
 * Return: 0 for success, nonzero for failure, though practically most errors
 * will be fatal and cause immediate exit.
 */
static int configureVmeEndpoints(void)
{
	int crate, fd;
	V120_HANDLE * hCrate;

	/* Sanity check the call. */
	if (len_crates != 0 || len_pollfds != 0) {
		logcrit("VME endpoints must be added first.");
		exit(1);
	}

	/* Open the interrupt endpoint for every crate that will let us.  Store
	 * those crates in v120_info, and their interrupt endpoints in list_pollfds.
	 */
	for (crate = 0; crate < 16; crate++) {
		hCrate = v120_open(crate);
		if (hCrate == NULL) continue;

		fd = v120_irq_open(hCrate);
		if (fd < 0) {
			v120_close(hCrate);
			continue;
		}

		v120_info[len_crates].handle = hCrate;
		v120_info[len_crates].irqhndl = v120_get_irq(hCrate);
		v120_info[len_crates].cratenumber = crate;
		if (append_fd(fd, POLLIN) == NULL) {
			logcrit("Failed adding VME interrupt endpoint to list: %s", strerror(errno));
			exit(1);
		}
		len_crates++;
	}
	return 0;
}

/**
 * configureServerSocket() - Add the server socket to list_pollfds.
 *
 * Must be called after adding all the VME fds, but before any clients.
 *
 * Return: 0 for success, nonzero for failure, though practically most errors
 * will be fatal and cause immediate exit.
 */
static int configureServerSocket(void)
{
	int serversocket;

	/* Sanity check the call. */
	if (len_crates != len_pollfds) {
		logcrit("Server socket must be added right after VME endpoints.");
		exit(1);
	}

	serversocket = v120irqd_server(NULL);
	if (serversocket < 0) {
		logcrit("Failed opening server socket: %s", strerror(errno));
		exit(1);
	}
	if (append_fd(serversocket, POLLIN) == NULL) {
		logcrit("Failed adding server socket to list: %s", strerror(errno));
		exit(1);
	}
	return 0;
}

/**
 * configureClientSocket() - Add a single client socket to list_pollfds.
 *
 * Must be called after both VME and server socket fds are added.
 *
 * Return: 0 for success, nonzero for failure, though practically most errors
 * will be fatal and cause exit.
 */
static int configureClientSocket(int fd)
{
	/* Sanity check the call. */
	if (len_crates >= len_pollfds) {
		logcrit("VME and server socket fds must be created before clients.\n");
		exit(1);
	}

	if (append_fd(fd, POLLIN) == NULL) {
		logcrit("Couldn't add client fd: %s", strerror(errno));
		return 1;
	}
	return 0;
}

/**********************************************************************
 * Polling loop processing functions
 **********************************************************************/

/**
 * notify_irq() - Find the registered listener and notify it about an IRQ.
 * @sel: A concrete v120irqd_selector describing the interrupt.
 *
 * Return: 0, or a negative error code to indicate a problem.
 */
static int notify_irq(struct v120irqd_selector * sel)
{
	int sock, err;
	sock = find_interrupt(sel);
	if (sock == 0) {
		if (errno == EINVAL) {
			logwarn("No target for %04X:%02X:%08X", sel->crate, sel->irq, sel->vector);
		}
		return -errno;
	}

	/* TODO: There should probably be some kind of timeout here, just to
	 * make sure that a poorly written client doesn't hang the system
	 * indefinitely.
	 */
	logdebug("Sending IRQ %04X:%02X:%08X", sel->crate, sel->irq, sel->vector);
	err = v120irqd_interrupt(sock, sel);
	return err;
}

/**
 * process_vme() -	Process all interrupts on a given crate.
 * @idx:	The list_pollfds index to the VME endpoint.
 *
 * process_vme() will keep iterating over the active IRQ list, fetching the
 * interrupt vector of the highest priority interrupt and dispatching it to
 * the registered target, until the all interrupts on the crate have been
 * cleared.
 */
static void process_vme(int idx)
{
	uint32_t irqen, irqstatus;
	unsigned int irq;
	int err;
	struct v120irqd_selector selector;

	/* If nothing else, we need to read the flag to reset the socket. */
	int fd = list_pollfds[idx].fd;
	err = read(fd, NULL, 1);

	V120_IRQ * irqhndl = v120_info[idx].irqhndl;

	/* The interrupt mask can't legally change while we do this, so cache it
	 * to avoid lots of long slow pulls.  We'll do the same thing with the
	 * irqstatus once per loop.
	 */
	irqen = irqhndl->irqen;

irqsearch:
	irqstatus = irqhndl->irqstatus & irqen;
	if (irqstatus == 0) return;

	for (irq = 7; irq >= 1; irq--) {
		/* If this bit isn't set then move on. */
		if ((irqstatus & (1 << irq)) == 0) continue;

		/* Retrieve the vector from VME, then kick it over to the client. */
		selector.vector = irqhndl->iack_vector[irq];
		selector.crate = (1 << v120_info[idx].cratenumber);
		selector.irq = (1 << irq);
		err = notify_irq(&selector);

		switch (err) {
			case 0: {
				goto irqsearch;
			}

			case -EINVAL: {
				/* We had no registered client for the interrupt.
				 * If we're lucky and the interrupt was obviously ROAK
				 * then we just keep going, otherwise we need to disable
				 * this interrupt.
				 */

				logwarn(
					"Targetless interrupt: Crate %d IRQ%d @0x%08X",
					v120_info[idx].cratenumber, irq, selector.vector
				);
			}

			case -EBADMSG: {
				/* The client NAKed the message.  Ummm, okay? */

				logwarn(
					"Client NAK: Crate %d IRQ%d @0x%08X",
					v120_info[idx].cratenumber, irq, selector.vector
				);
			}

			case -EPIPE: {
				/* The client connection closed unexpectedly; we'll
				 * notice this later when we get back to the poll loop.
				 *
				 * There's really no special handling for this versus
				 * any other error, but at least we understand it.
				 *
				 * Disable the interrupt for now; the reanalysis when the
				 * client is dropped may turn it back on later.
				 */
				break;
			}

			default: {
				/* We're stumped. */
				logerror("Error processing interrupt: %s", strerror(-err));
			}
		}

		if ((irqhndl->irqstatus & (1 << irq)) == 0) goto irqsearch;

		/* Well, this IRQ line is a bust; we can't do a thing with it
		 * and it's stuck on.  All we can do is keep from nuking the
		 * entire system.
		 */
		logwarn("Disabling unclearable interrupt %d.", irq);
		irqen &= ~(1 << irq);
		irqhndl->irqen = irqen;
		goto irqsearch;
	}

	/* This code should be absolutely unreachable.  Get here and it's fatal. */
	logcrit("IRQSTATUS 0x%X is rampantly illegal at %s:%d.",
		irqstatus, __FILE__, __LINE__
	);
	exit(1);
}

/**
 * process_newclient() - Accept a new client connection, add it to list_pollfds.
 */
static void process_newclient(void)
{
	int client = accept(list_pollfds[len_crates].fd, NULL, NULL);
	if (client < 0) {
		logerror("accept() failed: %s", strerror(errno));
		return;
	}
	if (configureClientSocket(client)) {
		close(client);
		return;
	}

	loginfo("Accepted client connection (%d)", count_clients());
	return;
}

/**
 * enable_interrupts() - Turn on all appropriate interrupt enables.
 *
 * For every crate set in @st->crate ensure that all the interrupts set in
 * @st->irq are enabled.  This is called when adding a new interrupt request
 * to the list.
 */
static void enable_interrupts(struct v120irqd_selector * st)
{
	/* Enable the correct IRQs on all the appropriate crates. */
	for (int idx = 0; idx < len_crates; idx++) {
		if (st->crate && (1 << v120_info[idx].cratenumber)) {
			uint32_t irq = v120_info[idx].irqhndl->irqen;
			v120_info[idx].irqhndl->irqen = irq | st->irq;
		}
	}
}

/**
 * disable_unused_interrupts() - Set interrupt enables to reflect the table.
 *
 * Go through all interrupts currently registered in the vector table, then
 * go through the crates setting the interrupt enable masks to only those
 * interrupts that are enabled.  This is called when removing interrupt
 * requests.
 */
static void disable_unused_interrupts(void)
{
	uint8_t irqs[16];
	list_registered_interrupts(irqs);
	for (int idx = 0; idx < len_crates; idx++) {
		int crate = v120_info[idx].cratenumber;
		v120_info[idx].irqhndl->irqen = irqs[crate];
	}
}

/**
 * build_status_report() - Get all the server status information.
 * @status: A buffer to hold the status information.
 */
static void build_status_report(struct v120irqd_serverstatus * status)
{
	status->pid = getpid();
	status->crates = 0;
	for (int idx=0; idx < len_crates; idx++) {
		status->crates |= (1 << v120_info[idx].cratenumber);
	}
	status->clients = count_clients();
	status->irq_requests = count_registered_interrupts();
}

/**
 * process_client() - Handle a (non-response) message from a client socket.
 * @idx:		The list_pollfds index to the client socket.
 * @revents:	The received socket events from the poll() call.
 */
static void process_client(int idx, int revents)
{
	int sock;
	int e;
	ssize_t len;
	response_buffer buffer;

	/* Read the incoming message. */
	sock = list_pollfds[idx].fd;
	len = v120_irqd_msg_recv(sock, &buffer);
	if (len < 0) {
		logerror("failed to get message: %s", strerror(-len));
		return;
	} else if (len == 0) {
		/* The client hung up. */
		remove_fd(idx);
		release_all_interrupts(sock);
		disable_unused_interrupts();
		close(sock);
		loginfo("Disconnected client (%d left)", count_clients());
		return;
	}

	switch (buffer.msg) {
	case REQUEST_IRQ:
		e = register_interrupt(sock, &buffer.selector);
		if (e < 0) {
			logerror("Failed to register interrupt: %s", strerror(-e));
			e = v120irqd_nak(sock);
			if (e < 0) {
				logerror("Error NAKing: %s", strerror(-e));
			}

		} else {
			e = v120irqd_ack(sock);
			if (e < 0) {
				logerror("Error ACKing: %s", strerror(-e));
			} else {
				enable_interrupts(&buffer.selector);
			}
		}
		break;

	case RELEASE_IRQ:
		e = release_interrupt(sock, &buffer.selector);
		if (e < 0) {
			logerror("Failed to release interrupt: %s", strerror(-e));
			e = v120irqd_nak(sock);
		} else {
			e = v120irqd_ack(sock);
		}

		if (e < 0) {
			logerror("Error acknowledging interrupt release: %s", strerror(-e));
		}

		disable_unused_interrupts();
		break;

	case IRQ_SIGNAL:
		/* Getting an IRQ_SIGNAL from a client is a request to fake an
		 * interrupt for debugging purposes.
		 */
		if (settings.allowFakeIrq) {
			e = v120irqd_ack(sock) || notify_irq(&buffer.selector);
			if (e < 0 && e != -EINVAL) {
				logerror("Couldn't signal fake interrupt: %s", strerror(-e));
			}
		} else {
			e = v120irqd_nak(sock);
			if (e < 0 && e != -EINVAL) {
				logerror("Couldn't NAK fake interrupt: %s", strerror(-e));
			}

		}
		break;

	case SERVER_STATUS:
		build_status_report(&buffer.status);
		v120irqd_msg_send(sock, &buffer);
		break;

	default:
		logerror("Bad message received: %s.\n", message_select_str(buffer.msg));
		break;
	}
}

/**********************************************************************
 * Main application
 **********************************************************************/

const char *program_version =
"V120 Interrupt Dispatcher " VERSION "\n"
"Copyright (C) 2015 Highland Technology, Inc.\n"
"Released under the 3-Clause BSD license <http://opensource.org/licenses/BSD-3-Clause>.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law.\n";

const char *program_usage =
"Usage: v120irqd [OPTIONS...]\n"
"\n"
"    -d, --debug        Debug mode: provid additional logging information.\n"
"    -f, --fakeok       Allow clients to send fake interrupts.\n"
"    -n, --novme        No MVE interrupts. Implies --fakeok.\n"
"    -k, --foreground   Run in foreground. Default is to fork to \n"
"                       background.\n"
"    -?, --help         Give this help list\n"
"    -V, --version      Print program version\n";

/**
 * parse_opt() - Command line option parser
 */
static void parse_opt(int key)
{
	switch (key) {
	case 'f':
		settings.allowFakeIrq = true;
		break;
	case 'k':
		settings.noDaemon = true;
		break;
	case 'd':
		settings.debugMode = true;
		break;
	case 'n':
	case '0':
		settings.noVME = true;
		parse_opt('f');
		break;
	case '?':
		printf("%s", program_usage);
		exit(EXIT_SUCCESS);
		break;
	case 'V':
		printf("%s", program_version);
		exit(EXIT_SUCCESS);
		break;
	default:
		fprintf(stderr, "invalid option '%c'\n", key);
		exit(EXIT_FAILURE);
		break;
	}
}

static void parseArgs(int argc, char * argv[])
{
	struct option options[] = {
		{ "debug",      no_argument, NULL, 'd' },
		{ "fakeok",     no_argument, NULL, 'f' },
		{ "novme",      no_argument, NULL, 'n' },
		{ "no-vme",     no_argument, NULL, '0' },
		{ "foreground", no_argument, NULL, 'k' },
		{ "help",       no_argument, NULL, '?' },
		{ "version",    no_argument, NULL, 'V' },
		{ NULL, 0, NULL, '\0' },
	};
	int opt;
	while ((opt = getopt_long(argc, argv, "dfn0k?V",
				  options, NULL)) != -1) {
		parse_opt(opt);
	}
}

static int configureSyslog(void)
{
	/* Syslog configuration. */
	openlog(NULL, LOG_PERROR, LOG_USER);
	if (settings.debugMode)		setlogmask(LOG_UPTO(LOG_DEBUG));
	else						setlogmask(LOG_UPTO(LOG_NOTICE));

	syslog(LOG_NOTICE, "Starting v120irqd " VERSION " %s ...",
		settings.allowFakeIrq ? "with --fakeok" : ""
	);
	return 0;
}

/**
 * termination() - Provide notice that the program was gracefully terminated.
 *
 * This is registered by atexit.
 */
static void termination(void)
{
	syslog(LOG_NOTICE, "Shutting down.");
}

/**
 * set_rtpriority() - Set maximum priority and lock all our pages in memory.
 *
 * Return: 0 on success or a negative error value.
 */
static int set_rtpriority(void)
{
#ifdef HAVE_MMAN_H
#  ifdef HAVE_SCHED_H
	const int policy = SCHED_FIFO;
	struct sched_param sp;
	sp.sched_priority = sched_get_priority_max(policy);
	if (sched_setscheduler(0, policy, &sp)) {
		return -errno;
	}
#  endif

	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
		return -errno;
	}
#endif
	return 0;
}

/**
 * initialize() - Initialize the application.
 *
 * Return: The non-zero return of any failing initialization function, or
 * 0 for complete success.
 */
static int initialize(int argc, char * argv[])
{
	int ret;
	parseArgs(argc, argv);
	if ((ret = configureSyslog()) != 0)			return ret;

	/* Build up a list of all the available file descriptors.  This
	 * list should contain (in this order):
	 * 	* All the VME interrupt endpoints
	 * 	* The base socket, used for accepting new connections.
	 * 	* The communications sockets.
	 */

	if (!settings.noVME) {
		if ((ret = configureVmeEndpoints()) != 0) return ret;
	}
	if ((ret = configureServerSocket()) != 0) return ret;

	/* Now the Linuxy stuff.  Set up a handler for SIGTERM and SIGUSR1, and
	 * block them whenever we're not waiting for our poll, then daemonize
	 * the process.
	 */
	ret = set_rtpriority();
	if (ret == -EPERM) {
		logwarn("Permission denied setting real-time priority: must run as root.");
		logwarn("Falling back to running at default priority.");
	} else if (ret != 0) {
		logerror("Failed to set real-time priority: %s", strerror(-ret));
		return 1;
	} else {
		logdebug("Successfully set real-time priority.");
	}

	sigset_t blockset;
	sigemptyset(&blockset);
	sigaddset(&blockset, SIGTERM);
	sigaddset(&blockset, SIGUSR1);
	sigprocmask(SIG_BLOCK, &blockset, NULL);

	struct sigaction sa = {
		.sa_handler = &signalhandler,
		.sa_mask = blockset
	};
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);

	atexit(&termination);

	if (!settings.noDaemon) {
		if (daemon(0, 0)) {
			logerror("Couldn't daemonize: %s", strerror(errno));
			fprintf(stderr, "Running in foreground instead...\n");
		}
	}
	return 0;
}

/**
 * signalhandler() - Flag the main loop when we get a signal.
 *
 * Only SIGTERM or SIGUSR1 should ever come in the door here, and the act of
 * getting this signal will break out of the ppoll loop, so the signals will
 * be blocked until we process them and call ppoll again.
 */
void signalhandler(int sig)
{
	caughtsignal = sig;
}

/**
 * mainloop() - One pass through the main application loop.
 *
 * Return: Non-zero to exit the loop, or 0 to continue.
 */
static int mainloop(void)
{
	int nevents;
	short revents;
	int idx;

	sigset_t emptyset;
	sigemptyset(&emptyset);

	/* Block until we get activity on any socket or a signal. */
	nevents = ppoll(list_pollfds, len_pollfds, NULL, &emptyset);

	if (nevents < 0) {
		if (errno == EINTR) {
			/* We caught a signal rather than a poll. */
			switch (caughtsignal) {
				case SIGTERM: {
					return 1;
					break;
				}
				case SIGUSR1: {
					struct v120irqd_serverstatus status;
					build_status_report(&status);
					syslog(
						LOG_ERR, "Crates: %d Clients %d Interrupts Registered %d",
						status.crates, status.clients, status.irq_requests
					);
					break;
				}
				default:{
					logerror("Bad signal handler %s", strsignal(caughtsignal));
					break;
				}
			}
			caughtsignal = 0;
			return 0;
		} else {
			logerror("Error in ppoll(): %s", strerror(errno));
			return 1;
		}
	}

	for (idx = 0; idx<len_pollfds && nevents; idx++) {
		revents = list_pollfds[idx].revents;
		if (revents) {
			/* Decode the pointer location. */
			if (idx < len_crates)			process_vme(idx);
			else if (idx == len_crates)		process_newclient();
			else							process_client(idx, revents);

			/* No matter what we did, we handled one event. */
			nevents--;
		}
	}
	return 0;
}

int main(int argc, char * argv[])
{
	int ret;

	ret = initialize(argc, argv);
	if (ret != 0) {
		logerror("Failed during intialization: %s", strerror(-ret));
		return 1;
	}

	logdebug("Waiting for connections.\n");
	while (ret == 0) {
		ret = mainloop();
	}
	return ret;
}
