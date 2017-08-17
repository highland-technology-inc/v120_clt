/**
 * DOC: Implementation of the V120 interrupt dispatch functions.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "v120irqd_intl.h"

/**********************************************************************
 * Low-level message send/receive functions
 **********************************************************************/

/* Send a message over a socket to a response_buffer. */
ssize_t v120irqd_msg_send(int socket, const response_buffer *buf)
{
	ssize_t len;
	len = write(socket, buf, sizeof(response_buffer));
	if (len < 0) {
		logwarn("Couldn't send message %s: %s", message_select_str(buf->msg), strerror(errno));
		return -errno;
	}
	return len;
}

/* Get a message from a socket into a response_buffer. */
ssize_t v120_irqd_msg_recv(int socket, response_buffer* buf)
{
	ssize_t len;
	len = read(socket, buf, sizeof(response_buffer));
	if (len < 0) {
		logwarn("Couldn't read message data from socket: %s", strerror(errno));
		return -errno;
	}
	return len;
}

/**********************************************************************
 * Utility functions
 **********************************************************************/

static char message_select_strbuf[16];

/* msg as a string */
const char * message_select_str(v120_irq_message_select msg)
{
	static const char* strs[] = {"NAK", "ACK", "REQUEST_IRQ", "RELEASE_IRQ", "IRQ_SIGNAL", "SERVER_STATUS"};
	if (msg >= NAK && msg <= IRQ_SIGNAL) {
		return strs[msg];
	} else {
		snprintf(message_select_strbuf, sizeof(message_select_strbuf), "%d", msg);
		return message_select_strbuf;
	}
}

/* -1 if x == 0 else floor(log2(x)) */
int v120irqd_ilog2f(uint32_t x)
{
	int ret = 0;
	if (x == 0) return -1;
	if (x & 0xFFFF0000) {ret += 16;	x >>= 16;}
	if (x & 0x0000FF00) {ret += 8;	x >>= 8;}
	if (x & 0x000000F0) {ret += 4;	x >>= 4;}
	if (x & 0x0000000C) {ret += 2;	x >>= 2;}
	if (x & 0x00000002) {ret += 1;}
	return ret;
}

/**********************************************************************
 * High level communications functions
 **********************************************************************/

/* Request interrupt notification from the server. */
int v120irqd_request(int socket, struct v120irqd_selector * sel)
{
	ssize_t len;
	response_buffer resp;
	unsigned err;

	/* Alright then, send the data requesting the IRQ. */
	resp.msg = REQUEST_IRQ;
	resp.selector = *sel;
	len = v120irqd_msg_send(socket, &resp);
	if (len < 0) return len;

	/* Make sure the server liked it, it'll respond with an ACK. */
	len = v120_irqd_msg_recv(socket, &resp);
	if (len < 0)					return len;
	else if (len == 0)				err = ECONNRESET;
	else if (resp.msg == ACK)		err = 0;
	else if (resp.msg == NAK)		err = EPERM;
	else 							err = EBADMSG;

	if (err) {
		errno = err;
		return -err;
	}
	return 0;
}

/* Stop previously requested interrupt notification from the server. */
int v120irqd_release(int socket, struct v120irqd_selector * sel)
{
	ssize_t len;
	response_buffer resp;
	unsigned err;

	/* Alright then, send the data requesting the IRQ. */
	resp.msg = RELEASE_IRQ;
	resp.selector = *sel;
	len = v120irqd_msg_send(socket, &resp);
	if (len < 0) return len;

	/* Make sure the server liked it, it'll respond with an ACK. */
	len = v120_irqd_msg_recv(socket, &resp);
	if (len < 0)					return len;
	else if (len == 0)				err = ECONNRESET;
	else if (resp.msg == ACK)		err = 0;
	else if (resp.msg == NAK)		err = EPERM;
	else 							err = EBADMSG;

	if (err) {
		errno = err;
		return -err;
	}
	return 0;
}

/* Retrieve the interrupt information pending on a socket. */
int v120irqd_getinterrupt(int socket, struct v120irqd_selector * sel)
{
	ssize_t len;
	response_buffer resp;
	unsigned err;

	len = v120_irqd_msg_recv(socket, &resp);
	if (len < 0)						return len;
	else if (len == 0)					err = ECONNRESET;
	else if (resp.msg == IRQ_SIGNAL)	err = 0;
	else 								err = EBADMSG;

	if (err) {
		errno = err;
		return -err;
	}

	*sel = resp.selector;
	return 0;
}

/* Send either an ACK or NAK. */
static int msg_respond(int socket, v120_irq_message_select response)
{
	response_buffer resp;
	ssize_t len;

	resp.msg = response;
	len = v120irqd_msg_send(socket, &resp);
	if (len < 0) return len;
	return 0;
}
int v120irqd_ack(int socket) { return msg_respond(socket, ACK); }
int v120irqd_nak(int socket) { return msg_respond(socket, NAK); }

/* Send and handshake (if block) an IRQ_SIGNAL on the socket. */
int v120irqd_interrupt(int socket, struct v120irqd_selector * sel)
{
	ssize_t len;
	response_buffer resp;
	unsigned err;

	resp.msg = IRQ_SIGNAL;
	resp.selector = *sel;
	len = v120irqd_msg_send(socket, &resp);
	if (len < 0) return len;

	len = v120_irqd_msg_recv(socket, &resp);
	if (len < 0)					return len;
	else if (len == 0)				err = ECONNRESET;
	else if (resp.msg == ACK)		err = 0;
	else if (resp.msg == NAK)		err = EPERM;
	else 							err = EBADMSG;

	if (err) {
		errno = err;
		return -err;
	}
	return 0;
}

/* Request the server status. */
int v120irqd_status(int socket, struct v120irqd_serverstatus *status)
{
	response_buffer resp = {0};
	ssize_t len;
	int err;

	resp.msg = SERVER_STATUS;
	len = v120irqd_msg_send(socket, &resp);
	if (len < 0) return len;

	len = v120_irqd_msg_recv(socket, &resp);
	if (len < 0)						return len;
	else if (len == 0)					err = ECONNRESET;
	else if (resp.msg == SERVER_STATUS)	err = 0;
	else 								err = EBADMSG;

	if (err) {
		errno = err;
		return -err;
	}

	*status = resp.status;
	return 0;
}

/**********************************************************************
 * Socket management utilities
 **********************************************************************/

#ifndef UNIX_PATH_MAX
#  define UNIX_PATH_MAX 92
#endif
struct sockaddr_un {
	sa_family_t sun_family;               /* AF_UNIX */
	char        sun_path[UNIX_PATH_MAX];  /* pathname */
};

/* Keep a linked list of filesystem server sockets so they can be deleted. */
static struct server_sockets {
	char name[UNIX_PATH_MAX];
	struct server_sockets *next;
} *server_socket_list = NULL;

/**
 * cleanupfunction() - Unlink any filesystem server sockets that were created.
 *
 * If v120irqd_server is called with socketname in the real filesystem, then
 * the filename will be added to the server_socket_list, and cleanupfunction()
 * registered for atexit().  On normal program exit, all the socket debris
 * should get cleaned up before termination.
 */
static void cleanupfunction(void)
{
	struct server_sockets *ptr = server_socket_list;
	while (ptr != NULL) {
		int err = unlink(ptr->name);
		if (err && (errno != ENOENT)) {
			logerror("couldn't unlink %s: %s", ptr->name, strerror(errno));
		}
		ptr = ptr->next;
	}
}

/* Returns an open socket for the server to use or -1. */
int v120irqd_server(const char *socketname)
{
	int sock;
	const size_t sos = sizeof(struct sockaddr_un);
	struct sockaddr_un server_socket_info;
	server_socket_info.sun_family = AF_UNIX;

	if (socketname == NULL) socketname = DEFAULTSOCKET;
	strncpy(server_socket_info.sun_path, socketname, UNIX_PATH_MAX);

	/* Unix domain sockets starting with a null are treated specially, they
	 * do not appear in the file system and so don't need to be unlinked.
	 * Filesystem sockets, however, take a bit more work.
	 */
	if (*server_socket_info.sun_path == '@') {
		*server_socket_info.sun_path = '\0';
	} else {
		/* Unlink it if it already exists. */
		int err = unlink(server_socket_info.sun_path);
		if (err && (errno != ENOENT)) {
			logerror("couldn't unlink %s: %s", server_socket_info.sun_path, strerror(errno));
			return -1;
		}

		/* Add this socket to the cleanup list, and register the cleanup
		 * function if this is the first socket we're adding.
		 */
		if (server_socket_list == NULL) {
			atexit(&cleanupfunction);
		}

		struct server_sockets *newss = malloc(sizeof(struct server_sockets));
		if (newss == NULL) {
			logerror("malloc failed: %s", strerror(errno));
			return -1;
		}

		strncpy(newss->name, socketname, UNIX_PATH_MAX);
		newss->next = server_socket_list;
		server_socket_list = newss;
	}

	/* Bind the address to the socket and put it into listen mode. */
	sock = socket(server_socket_info.sun_family, SOCK_SEQPACKET, 0);
	if (sock < 1) return -1;

	if (bind(sock, (struct sockaddr *)&server_socket_info, sos)) {
		logerror("bind() failed: %s", strerror(errno));
		goto cleanup;
	}
	if (listen(sock, 0)) {
		logerror("listen() failed: %s", strerror(errno));
		goto cleanup;
	}
	return sock;

cleanup:
	close(sock);
	return -1;
}

/* Returns an open socket for the client to use or -1. */
int v120irqd_client(const char* socketname)
{
	int sock;
	const size_t sos = sizeof(struct sockaddr_un);
	struct sockaddr_un server_socket_info;
	server_socket_info.sun_family = AF_UNIX;

	if (socketname == NULL) socketname = DEFAULTSOCKET;
	strncpy(server_socket_info.sun_path, socketname, UNIX_PATH_MAX);
	if (*server_socket_info.sun_path == '@') *server_socket_info.sun_path = '\0';

	sock = socket(server_socket_info.sun_family, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (sock < 1) return -1;

	if (connect(sock, (struct sockaddr *)&server_socket_info, sos)) {
		logerror("connect() failed: %s", strerror(errno));
		goto cleanup;
	}
	return sock;

cleanup:
	close(sock);
	return -1;
}
