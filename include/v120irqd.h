/**
 * DOC: Library header for V120 VME interrupt dispatch service.
 *
 * This is the library header for clients to receive VME interrupts as
 * asynchronous events by using the v120irqd service.
 *
 * When opening the Unix domain socket, a NULL pointer can be provided to just
 * use the compiled in default of DEFAULTSOCKET. To use a DEFAULTSOCKET in the
 * Linux abstract namespace, rather than on the filesystem, start the socket
 * name with an @ character; this is done in the default DEFAULTSOCKET of
 * "@/v120/v120irqd"
 *
 * This comes with the advantage of not needing to unlink old versions of the
 * socket, which allows the server to automatically enforce the idea that only
 * one copy of the server should be running at once.
 *
 * Many functions will say they return "Standard success."  What that means in
 * the context of this library is that they will return 0 on success, or a
 * negative number on failure, which will be -errno at the time of return.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#ifndef V120IRQD_H
#  define V120IRQD_H 1

#include <stdint.h>
#include <sys/types.h>

/**********************************************************************
 * Parameters
 **********************************************************************/

#ifndef DEFAULTSOCKET
#  define DEFAULTSOCKET "@/v120/v120irqd"
#endif

/**********************************************************************
 * Types
 **********************************************************************/

/**
 * struct v120irqd_selector - Describes a request for interrupt handling.
 * @crate:		Crate bitmask.	(1<<n) registers for crate n (0-15).
 * 								ANYCRATE for all.
 * @irq:		IRQ bitmask.	(1<<n) registers for IRQn (1-7).
 * 								ANYIRQ for all.
 * @vector:		IACK vector response from card, or ANYVECTOR for all.
 * @payload:	User defined data to be returned when this request hits.
 *
 * Crate and irq are both bitmasks.  Crate uses 0x0001 through 0x8000 to
 * represent crate 0 through crate 15 respectively.  Irq uses 0x02 through
 * 0x80 to represent VMEbus IRQ1* through IRQ7*.  When requesting an interrupt,
 * you can use this as "multibit" selector, one that takes advantage
 * of the bitmask to request, for instance, any interrupt that may occur on
 * crates 3 and 6.  The ANYCRATE and ANYIRQ constants are defined to simplify
 * broader requests.
 *
 * The vector field is a number that must correspond directly to the interrupt
 * vector queried from the backplane.  This vector is always 32-bits, even if
 * the VME interrupter being queried is only D16 or D8.  Unused bits must be
 * padded with 1s, not zeros, to reflect the fact that the VME backplane is
 * idle-high.  So a D16 module returning vector 0x1234 would have a .vector of
 * 0xFFFF1234.  The special case of ANYVECTOR can be used with a multibit
 * selector to request delivery of the interrupt regardless of retrieved vector.
 *
 * A delivered interrupt, on the other hand, will always be "concrete", which
 * is to say that it will have only 1 bit set in crate and 1 bit set in IRQ, and
 * the vector will be the actual value fetched from the backplane, which should
 * never be ANYVECTOR.
 *
 * The payload is arbitrary client data, provided to the server then given back
 * when requested interrupts are matched.  The idea is that this can be an index
 * into a table allowing for quick dispatching when an interrupt is returned,
 * rather than the client having to once again iterate looking for the correct
 * crate/irq/vector combination.  Note that on a 32-bit system, the payload is
 * sufficient to hold a function pointer.  DO NOT do this; it's a giant glaring
 * security hole.  A table index can be easily range checked for validity before
 * just executing any old code; running whatever function pointer you're handed
 * back is like eating candy from strangers leaning out of panel vans.
 */
struct v120irqd_selector {
	uint16_t crate;
	uint16_t irq;
	uint32_t vector;
	uint32_t payload;
};
#define ANYCRATE	(0xFFFF)
#define ANYIRQ		(0x00FE)
#define ANYVECTOR	(0xFFFFFFFF)

/**
 * struct v120irqd_serverstatus - Information about the server for clients.
 * @pid:			Process ID of the server.
 * @crates:			Bitmask of available VME crates (0-15).
 * @clients:		Number of clients connected, including the one asking.
 * @irq_requests:	Number of registered IRQ descriptions.
 */
struct v120irqd_serverstatus {
	pid_t pid;
	unsigned int crates;
	unsigned int clients;
	unsigned int irq_requests;
};

/**********************************************************************
 * Declaration of the functions in interrupt.c
 **********************************************************************/

/**
 * v120irqd_ilog2f() - floor(log2(x)) for integer x.
 * @x: A bitmask.
 * 
 * This corresponds to the index of the highest bit set, so
 * v120irqd_ilog2f(16) == v120irqd_ilog2f(31) == 4.
 *
 * v120irqd_ilog2f(0) = -1 by definition.
 *
 * End users don't strictly need this function, but it's a nice convenience
 * when dealing with bitmasks.
 */
extern int v120irqd_ilog2f(uint32_t x);

/**
 * v120irqd_client() - Open a connection to the server.
 * @socketname:		Name of the socket in the filesystem.  Begin it with an
 * 					'@' to make it a hidden Linux socket (preferable because
 *					it cleans up after itself).  NULL to use the compile-time
 * 					default; which should be the case anytime other than weird
 * 					testing scenarios.
 *
 * Return: A file descriptor for the socket, or -1 for failure and errno is set.
 */
extern int v120irqd_client(const char *socketname);

/**
 * v120irqd_request() - Request interrupt notification from the server.
 * @socket:		The open connection to the server.
 * @sel:		A description of the interrupt to be notified on.
 *
 * The server can only have one recipient for any given interrupt, and will
 * NAK if any attempt is made to request a notification that has already been
 * assigned.  If you are going to request both specific and general handlers,
 * make the specific requests first.  For instance, request
 *   {.crate = (1 << 0), .irq = (1 << 5), .vector = 0xDEADBEEF}
 * before you request
 *   {.crate = (1 << 0), .irq = ANYIRQ, .vector = ANYVECTOR}.
 *
 * Return: Standard success. Specifically, -EPERM will be returned for a server
 * NAK, which will be received if any of the selection fields have an invalid
 * value or conflict with existing interrupts.
 */
extern int v120irqd_request(int socket, struct v120irqd_selector * sel);

/**
 * v120irqd_release() - Stop getting a given interrupt notification.
 * @socket:		The open connection to the server.
 * @sel:		The interrupt notification to stop receiving.
 * 				Must be identical to the one requested during v120irqd_request().
 *
 * Return: Standard success.
 */
extern int v120irqd_release(int socket, struct v120irqd_selector * sel);

/**
 * v120irqd_getinterrupt() - Copy interrupt notification into @sel.
 * @socket:		The open connection to the server.
 * @sel:		On success, the description of the received interrupt.
 *
 * The call blocks until an interrupt notification is sent from the server.
 * poll() can be used to not call this function until a message is available.
 *
 * The client must reply with v120irqd_ack() or v120irqd_nak() after receiving an
 * interrupt notification or the entire interrupt system will stop working.
 * In the case of an ROAK interrupt, the client should do this immediately.
 * In the case of an RORA interrupt, the client should perform the necessary
 * register access to clear the interrupt as soon as possible, then acknowledge.
 *
 * Return: Standard success.  Specifically, -EBADMSG indicates that while a
 * successful message was received, it wasn't an interrupt. -ENOTCONN indicates
 * the socket is not connected.
 */
extern int v120irqd_getinterrupt(int socket, struct v120irqd_selector *sel);

/**
 * v120irqd_interrupt() - Signal an IRQ on the socket.
 * @socket:		The open connection to the client/server.
 * @sel:		The interrupt information to be sent.
 *
 * The server calls this to send an interrupt notification to a client. Clients
 * can call this to send a request for a fake interrupt to the server for
 * testing purposes if the server was started with --fakeok.
 *
 * Block until a response is returned; fail if that response is not ACK.
 *
 * Return: Standard success.  Specifically, -EBADMSG indicates that the client
 * sent a response other than an ACK.
 */
extern int v120irqd_interrupt(int socket, struct v120irqd_selector *sel);

/**
 * v120irqd_ack() - Send an affirmative (ACK) response.
 * @socket:		The open connection to the client/server.
 *
 * Return: Standard success.
 */
extern int v120irqd_ack(int socket);

/**
 * v120irqd_ack() - Send a negative (NAK) response.
 * @socket:		The open connection to the client/server.
 *
 * Return: Standard success.
 */
extern int v120irqd_nak(int socket);

/**
 * v120irqd_status() - Query the server status.
 * @socket:		The open socket to the server.
 * @status:		Buffer to store the returned information.
 *
 * Return: Standard success.
 */
extern int v120irqd_status(int socket, struct v120irqd_serverstatus *status);

#endif
