/**
 * DOC: Library header for V120 VME interrupt dispatch service internals.
 *
 * This header is used by the interrupt library internals, and by the server.
 * Client applications should have no need of it.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#ifndef V120IRQD_INTL_H
#  define V120IRQD_INTL_H 1

#include <syslog.h>
#include "v120irqd.h"

/**
 * enum v120_irq_message_select - Serialized message type identifier.
 * @NAK:			Receipt acknowledged, action failed.
 * 					Either direction, no payload.
 * @ACK:			Receipt acknowledged, action successful.
 * 					Either direction, no payload.
 * @REQUEST_IRQ:	Request notification on an IRQ.
 * 					Acknowledgement required.
 * 					Client->server, Multibit v120irqd_selector payload.
 * @RELEASE_IRQ:	Drop request for IRQ notification.
 * 					Acknowledgement required.
 * 					Client->server, Multibit v120irqd_selector payload that must
 * 					match exactly the one from the @REQUEST_IRQ.
 * @IRQ_SIGNAL:		Signal an IRQ has occurred.
 * 					Acknowledgement required.
 * 					Server->client, this is the interrupt notification.
 * 					Client->server, this is a request for a fake interrupt.
 * 					Concrete v120irqd_selector payload.
 * @SERVER_STATUS:	Client->server, this is a request for server information.
 * 					Server->client, this is the response with a
 * 					v120irqd_serverstatus payload.
 */
typedef enum v120_irq_message_select {
	NAK, ACK,
	REQUEST_IRQ, RELEASE_IRQ,
	IRQ_SIGNAL,
	SERVER_STATUS
} v120_irq_message_select;

/**
 * struct response_buffer - General purpose communications buffer.
 * @msg:		The message type identifier.
 * @selector:	Payload for REQUEST_IRQ, RELEASE_IRQ, IRQ_SIGNAL.
 * @status:		Payload for SERVER_STATUS.
 */
typedef struct response_buffer {
	v120_irq_message_select msg;
	union {
		struct v120irqd_selector selector;
		struct v120irqd_serverstatus status;
	};
} response_buffer;

/**
 * v120irqd_msg_send() - Send an arbitrary message to a socket.
 *
 * Return: The total bytes sent, or a negative error code.
 */
ssize_t v120irqd_msg_send(int socket, const response_buffer *buf);

/**
 * v120_irqd_msg_recv() - Get an arbitrary message from a socket.
 *
 * Return: The total bytes read, or a negative error code.
 */
ssize_t v120_irqd_msg_recv(int socket, response_buffer* buf);

/**
 * v120irqd_server() - Create an open socket in listen mode.
 * @socketname		Name of the socket in the filesystem.  Begin it with an
 * 					'@' to make it a hidden Linux socket (preferable because
 *					it cleans up after itself).  NULL to use the compile-time
 * 					default; which should be the case anytime other than weird
 * 					testing scenarios.
 *
 * Return: A file descriptor for the socket, or -1 for failure and errno is set.
 */
int v120irqd_server(const char *socketname);

/**
 * message_select_str() - Return the constant name for a v120_irq_message_select.
 *
 * If v120_irq_message_select is not valid then this function is not strictly
 * thread-safe.  Such is life.
 */
const char * message_select_str(v120_irq_message_select msg);

/**********************************************************************
 * Error reporting tools.
 **********************************************************************/

/**
 * DOC: Syslog diagnostic macros
 * logdebug, loginfo, logwarn, logerror, and logcrit are all thin wrappers
 * around GNU syslog functions that allow the server to place diagnostic
 * information into the syslog.
 */
#define logdebug(msg, ...)	syslog(LOG_DEBUG,	"DEBUG: %s " msg, __func__, ##__VA_ARGS__)
#define loginfo(msg, ...)	syslog(LOG_INFO,	"INFO: %s " msg, __func__, ##__VA_ARGS__)
#define logwarn(msg, ...)	syslog(LOG_WARNING,	"WARNING: %s " msg, __func__, ##__VA_ARGS__)
#define logerror(msg, ...)	syslog(LOG_ERR,		"ERROR: %s "  msg, __func__, ##__VA_ARGS__)
#define logcrit(msg, ...)	syslog(LOG_CRIT,	"FAILURE: %s "  msg, __func__, ##__VA_ARGS__)

#endif
