/**	
 * DOC: Manage the IRQ registration tables for v120irqd.
 *
 * The management of the tables is something of a self-contained task, which
 * is somewhat orthogonal to the server logic itself, so it's been pulled out
 * into a separate file.  The tables relate a set of requested interrupts to
 * the client that requested them.
 *
 * Several functions work with irqdata_t elements, which are (at present) a
 * typedef alias of intptr_t (though this has changed a couple times through
 * development).  The idea here is that, for a simple single thread server, the
 * table can store just the socket file descriptor, whereas for a more
 * complicated multithreaded server, with need for mutex locking or the like on
 * a socket by socket basis, the table can store a pointer into some kind of
 * structure holding all of the additional information attached to the socket.
 *
 * The table is stored as a file-level global in irq_vector_table.c, and as
 * a result the server can have only one of them.  This is not envisioned to
 * be a problem.
 *
 * As with v120irqd.h, "standard success" means 0 on success or a negative error
 * code (-errno) on failure.
 * 
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 * 
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#ifndef IRQ_VECTOR_TABLE_H
#  define IRQ_VECTOR_TABLE_H 1

#include "v120irqd.h"

typedef intptr_t irqdata_t;

/**
 * count_registered_interrupts() - Count interrupts currently registered.
 * 
 * This is primarily for the benefit of the unit tests.
 */
unsigned count_registered_interrupts(void);

/**
 * register_interrupt() - Register client information for a given interrupt.
 *
 * This will create a table entry (enlarging the table if necessary) that
 * maps @sd to interrupts that match @request.
 * 
 * Return: Standard success.  Specifically, -EADDRINUSE will be returned for a
 * request code that is already mapped, and -EINVAL if sd is zero.
 */
int register_interrupt(irqdata_t sd, const struct v120irqd_selector * request);

/**
 * find_interrupt() - Find the interrupt request record matching the request.
 * @request:	The IRQ request to be searched for.  If a match is found, the
 * 				payload member will be updated to the client requested payload
 * 				for this match.
 * 
 * Crate and irq must have all bits set in the request, vector must
 * match exactly unless the registered vector is ALLVECTOR.
 *
 * Return: The server data identifying the client connection, or 0 if no
 * match was found or an error occurred.  errno will be EINVAL if no match was
 * found, or some other error code in case of an internal problem.
 */
irqdata_t find_interrupt(struct v120irqd_selector * request);

/**
 * release_interrupt() - Unregister one interrupt.
 * 
 * @request must have the exact same crate/irq/vector as was provided
 * to register_interrupt().
 *
 * Return: Standard success.  Specifically, if the given data/request
 * combination is not found, -EINVAL is returned.
 */
int release_interrupt(irqdata_t sd, const struct v120irqd_selector * request);

/**
 * release_all_interrupts() - Release all interrupts claimed by a given client.
 * 
 * This is primarily useful when a client closes its connection and we need to
 * clean up after it.
 *
 * Return: Standard success.
 */
int release_all_interrupts(irqdata_t sd);

/**
 * free_all_interrupts() - Free the entire interrupt vector table.
 * 
 * This is probably only useful for unit tests.
 */
void free_all_interrupts(void);

/**
 * list_registered_interrupts() - List all crate/IRQ combinations registered.
 * @irqs:	An array of 16 IRQ bitmasks.  irqs[n] will be loaded with the
 * 			bitmask of registered interrupts for crate n.
 * 
 * Example: If crate 5 has interrupts registered for IRQ2 and IRQ3, and crate 7
 * 			has registered ANYIRQ, then list_registered_interrupts will set
 * 			irqs[5] = 0x0C, irqs[7] = 0xFE, and irqs[0-4, 6, 8-15] = 0.
 */
void list_registered_interrupts(uint8_t irqs[16]);

#endif
