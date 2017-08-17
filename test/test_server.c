/*
 * A test suite for the v120 interrupt dispatcher program.  The program is
 * started as a separate process, client sockets are created from the suite
 * to the daemon.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "v120irqd.h"

#include "unity/unity.h"

#define SAFETY_ALARM 2
#define KILL_EXISTING_SERVER 1
#define USESOCKET NULL

/**********************************************************************
 * Test suite
 **********************************************************************/

#define TEST_NOFAIL(x)									\
	do {												\
		int _le = (x);									\
		TEST_ASSERT_FALSE_MESSAGE(_le, strerror(-_le));	\
	} while (0)

struct irq_request {
	int crate, irq;
	uint32_t vector;
	char * msg;
};
#define BIT(n) (1 << (n))

static struct irq_request *irq_request_options[] = {
	(struct irq_request[]){
		{BIT(0), BIT(1), 0xDEADBEEF,	"C0 dead beef"},
		{BIT(0), BIT(1), 0xFEEDD00D,	"C0 feed the man"},
		{BIT(0), BIT(1), ANYVECTOR,		"C0 IRQ1 remaining"},
		{BIT(0), BIT(3), ANYVECTOR,		"C0 IRQ3"},
		{0, 0, 0}
	},
	(struct irq_request[]){
		{ANYCRATE, BIT(7), ANYVECTOR,	"Any IRQ7"},
		{0, 0, 0}
	},
	(struct irq_request[]){
		{BIT(1), BIT(1), 0xDEADBEEF,	"C1 deader beef"},
		{BIT(1), BIT(1), 0xFEEDD00D,	"C1 hungrier man"},
		{BIT(1), BIT(3), ANYVECTOR,		"C1 IRQ3"},
		{BIT(1), BIT(4), ANYVECTOR,		"C1 IRQ4"},
		{0, 0, 0}
	}
};
#define NCLIENTS (sizeof(irq_request_options)/sizeof(*irq_request_options))

/** Get the total number of IRQ requests that should be registered by default. */
unsigned int total_requests(void) {
	struct irq_request * ptr;
	unsigned total = 0;

	for (int client=0; client<NCLIENTS; client++) {
		ptr = irq_request_options[client];
		while (ptr->crate != 0) {
			total++;
			ptr++;
		}
	}
	return total;
}

static struct pollfd fds[NCLIENTS];

/** Register all clients for their default interrupts. */
static void register_default_interrupts(void) {
	struct irq_request * ptr;
	struct v120irqd_selector req;
	unsigned payload = 0;
	int err, sock;
	char msg[128];


	for (int client=0; client<NCLIENTS; client++) {
		ptr = irq_request_options[client];
		sock = fds[client].fd;

		while (ptr->crate != 0) {
			req.crate = ptr->crate;
			req.irq = ptr->irq;
			req.vector = ptr->vector;
			req.payload = payload;
			err = v120irqd_request(sock, &req);

			if (err) {
				snprintf(
					msg, sizeof(msg),
					"Interrupt request client %d #%d failed: %s",
					client, payload, strerror(errno)
				);
				TEST_FAIL_MESSAGE(msg);
			}

			payload++;
			ptr++;
		}
	}
}

/**
 * Create sockets for all our clients, and register them into the fds list.
 */
void setUp(void) {
	char msg[128];

	for (int idx=0; idx < NCLIENTS; idx++) {
		int sock = v120irqd_client(USESOCKET);
		if (sock < 0) {
			snprintf(
				msg, sizeof(msg),
				"Couldn't open client %d: %s", idx, strerror(errno)
			);
			TEST_FAIL_MESSAGE(msg);
		}
		fds[idx].fd = sock;
		fds[idx].events = POLLIN;
	}

	register_default_interrupts();
}

/**
 * Close all our socket connections and make sure the server knows they're
 * all gone.
 */
void tearDown(void) {
	int sock, err;
	char msg[128];
	struct v120irqd_serverstatus status;

	for (int idx=0; idx < NCLIENTS; idx++) {
		sock = fds[idx].fd;
		if (sock) {
			close(sock);
			fds[idx].fd = 0;
		}
	}

	sock = v120irqd_client(USESOCKET);
	if (sock < 0) {
		snprintf(
			msg, sizeof(msg),
			"Couldn't open query connection: %s", strerror(errno)
		);
		TEST_FAIL_MESSAGE(msg);
	}

	msg[0] = '\0';
	err = v120irqd_status(sock, &status);
	if (err) {
		snprintf(
			msg, sizeof(msg),
			"Server status failed: %s", strerror(errno)
		);
	} else if (status.clients != 1) {
		snprintf(
			msg, sizeof(msg),
			"Expected only the query client, instead got %d", status.clients
		);
	}

	close(sock);
	if (msg[0] != '\0') {
		TEST_FAIL_MESSAGE(msg);
	}
}

/** Confirm that the server sees all our connections. */
void test_connections(void) {
	struct v120irqd_serverstatus status;
	TEST_ASSERT_MESSAGE(
		!v120irqd_status(fds[0].fd, &status),
		"Couldn't get server status"
	);
	TEST_ASSERT_EQUAL(3, status.clients);
	TEST_ASSERT_EQUAL_HEX16(0, status.crates);
	TEST_ASSERT_EQUAL(total_requests(), status.irq_requests);
}

/** Confirm that illegal requests to add interrupts are NAKed. */
void test_illegal_request(void) {
	int err, sock;
	struct v120irqd_selector req;
	const struct v120irqd_selector legal = {
		.crate = BIT(4), .irq=BIT(5), .vector=0x12345678, .payload=0
	};
	struct v120irqd_serverstatus status;

	sock = fds[0].fd;
	req = legal;
	req.crate = 0;
	err = v120irqd_request(sock, &req);
	TEST_ASSERT_EQUAL_MESSAGE(-EPERM, err, "No NAK on illegal zero crate.");

	req = legal;
	req.irq = BIT(8);
	err = v120irqd_request(sock, &req);
	TEST_ASSERT_EQUAL_MESSAGE(-EPERM, err, "No NAK on illegal irq number.");
	req.irq = 0;
	err = v120irqd_request(sock, &req);
	TEST_ASSERT_EQUAL_MESSAGE(-EPERM, err, "No NAK on illegal zero irq.");

	req = legal;
	req.irq = BIT(7);
	err = v120irqd_request(sock, &req);
	TEST_ASSERT_EQUAL_MESSAGE(-EPERM, err, "No NAK on conflict.");

	err = v120irqd_status(sock, &status);
	TEST_NOFAIL(err);
	TEST_ASSERT_EQUAL_MESSAGE(total_requests(), status.irq_requests, "Added illegal requests.");
}

/** Try to remove a client 2 vector from client 2. */
void test_legal_removal(void)
{
	const struct irq_request *ptr = &irq_request_options[2][1];
	const int sock = fds[2].fd;

	struct v120irqd_selector req;
	struct v120irqd_serverstatus status;

	TEST_ASSERT(NCLIENTS >= 3);
	req.crate = ptr->crate;
	req.irq = ptr->irq;
	req.vector = ptr->vector;

	TEST_NOFAIL(v120irqd_release(sock, &req));
	TEST_NOFAIL(v120irqd_status(sock, &status));
	TEST_ASSERT_EQUAL(total_requests()-1, status.irq_requests);
}

/** Try to remove a client 2 vector from client 1. */
void test_illegal_removal(void)
{
	const struct irq_request *ptr = &irq_request_options[2][1];
	const int sock = fds[1].fd;

	int err;
	struct v120irqd_selector req;
	struct v120irqd_serverstatus status;

	TEST_ASSERT(NCLIENTS >= 3);
	req.crate = ptr->crate;
	req.irq = ptr->irq;
	req.vector = ptr->vector;
	err = v120irqd_release(sock, &req);
	TEST_ASSERT_EQUAL(-EPERM, err);

	TEST_NOFAIL(v120irqd_status(sock, &status));
	TEST_ASSERT_EQUAL(total_requests(), status.irq_requests);
}

const static struct irq_request *concrete[] = {
	(struct irq_request[]){
		{BIT(0), BIT(1), 0xDEADBEEF,	""},
		{BIT(0), BIT(1), 0xFEEDD00D,	""},
		{BIT(0), BIT(1), 0x12345678,	""},
		{BIT(0), BIT(3), 0x87654321,	""},
		{0, 0, 0}
	},
	(struct irq_request[]){
		{BIT(5), BIT(7), 0xABCDEF10,	""},
		{0, 0, 0}
	},
	(struct irq_request[]){
		{BIT(1), BIT(1), 0xDEADBEEF,	""},
		{BIT(1), BIT(1), 0xFEEDD00D,	""},
		{BIT(1), BIT(3), 0xCCAAACAC,	""},
		{BIT(1), BIT(4), 0xBEEF1111,	""},
		{0, 0, 0}
	}
};

/* This handler is null, but means that SIGALRM will break a read. */
void alarm_handler(int sig)
{
}

/** Confirm that we get all our interrupts where we should. */
void test_irq_receipt(void)
{
	struct v120irqd_selector req;
	int tgt, vec, payload;
	int xmit;
	int i;

	xmit = fds[0].fd;
	payload = 0;
	for (tgt=0; tgt<NCLIENTS; tgt++) {
		vec = 0;
		while(concrete[tgt][vec].crate != 0) {
			req.crate = concrete[tgt][vec].crate;
			req.irq = concrete[tgt][vec].irq;
			req.vector = concrete[tgt][vec].vector;

			/* Send a fake IRQ on the XMIT socket. */
			alarm(SAFETY_ALARM);
			TEST_NOFAIL(v120irqd_interrupt(xmit, &req));
			alarm(0);

			/* Confirm that only the one target socket received it. */
			TEST_ASSERT_EQUAL(1, poll(fds, NCLIENTS, 1000));
			for (i=0; i<NCLIENTS; i++) {
				TEST_ASSERT_EQUAL(i == tgt ? POLLIN : 0, fds[i].revents);
			}

			/* Get the interrupt on the selected port. */
			alarm(SAFETY_ALARM);
			TEST_NOFAIL(v120irqd_getinterrupt(fds[tgt].fd, &req));
			alarm(0);
			TEST_NOFAIL(v120irqd_ack(fds[tgt].fd));

			/* Confirm correct receipt. */
			TEST_ASSERT_EQUAL_HEX16(req.crate, concrete[tgt][vec].crate);
			TEST_ASSERT_EQUAL_HEX8(req.irq, concrete[tgt][vec].irq);
			TEST_ASSERT_EQUAL_HEX32(req.vector, concrete[tgt][vec].vector);
			TEST_ASSERT_EQUAL(req.payload, payload);

			vec++;
			payload++;
		}
	}
}

void test_alarm(void)
{
	/* Make sure that a SIGALRM breaks us out of an infinite wait in
	 * v120irqd_getinterrupt.
	 */

	int sock = fds[0].fd;
	struct v120irqd_selector req;

	alarm(1);
	TEST_ASSERT_EQUAL(-EINTR, v120irqd_getinterrupt(sock, &req));
}

/**********************************************************************
 * Test harness
 **********************************************************************/

/**
 * Shut down the server process if it's running.  Abort the program if any
 * problems occur.
 */
void kill_server(void) {
	int sock;
	struct v120irqd_serverstatus status;

	sock = v120irqd_client(USESOCKET);
	if (sock < 0) return;

	if (v120irqd_status(sock, &status)) {
		perror("Couldn't get server status");
		exit(1);
	}

	if (kill(status.pid, SIGTERM)) {
		perror("Couldn't set SIGTERM to server");
		exit(1);
	}

	close(sock);
}

int main(void)
{
	int err;

	if (KILL_EXISTING_SERVER) {
		/* If the server is already running we need to shut it down. */
		kill_server();

		/* Now start it fresh in a background process. */
		err = system(DAEMON_LOCAL_NAME " --novme --debug");
		if (err == -1) {
			perror("Couldn't start server.");
			exit(1);
		} else if (err && KILL_EXISTING_SERVER) {
			fprintf(stderr, "Server exited with error code %d\n", err);
			exit(1);
		}

		/* Great.  Server is running now.  Register a handler to get rid of it
		 * on the way out the door later. */
		atexit(&kill_server);
	}

	/* Prepare our alarm hander so we can use timeouts to get out of jams. */
	struct sigaction alarmaction = {
		.sa_handler = &alarm_handler,
		.sa_flags = SA_NODEFER
	};
	if (sigaction(SIGALRM, &alarmaction, NULL)) {
		perror("Couldn't register SIGALRM.");
		exit(1);
	}

	/* Run our test suite. */
	UnityBegin(__FILE__);

	RUN_TEST(test_connections);
	RUN_TEST(test_illegal_request);
	RUN_TEST(test_legal_removal);
	RUN_TEST(test_illegal_removal);
	RUN_TEST(test_alarm);
	RUN_TEST(test_irq_receipt);

	return UnityEnd();
}
