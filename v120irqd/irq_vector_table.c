/**
 * DOC: Implementation of the v120irqd IRQ vector tables.
 *
 * The table is currently stored as a single array, malloced in chunks defined
 * as INTERRUPT_VECTOR_INCR.  This keeps the entire table physically adjacent
 * in hopes of improving cache performance, at the expense of causing fairly
 * slow additions and removals to the table.  Since the table is likely to be
 * configured once at startup and never touched again, this seems like a good
 * trade.
 *
 * Other implementation options, which may be more or less efficient, include:
 *		* A linked list.  This was rejected because the extra storage
 * 		requirement hurts locality.
 *
 * 		* A 2D array of linked lists, indexed on the crate and IRQ number.  This
 * 		was rejected due to the additional work it would create for dubious
 * 		improvement.
 *
 * While there is only one global table for the program, accesses to it can be
 * made threadsafe by defining THREADSAFE for the compilation of this object,
 * and linking with pthread.  This will cause all accesses to the table to be
 * mutex locked.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>

#include "v120irqd_intl.h"
#include "irq_vector_table.h"
#include "config.h"

/***********************************************************************
 * DOC: External definitions
 *
 * THREADSAFE: Defining THREADSAFE inserts calls to pthread mutex locking
 * funtions around accesses to the interrupt table.  If v120irqd is
 * ever rewritten to be reentrant, define THREADSAFE and link with
 * -lpthread.
 *
 * INTERRUPT_VECTOR_INCE: The number of vectors to malloc at a time when
 * increasing table size.  Making this number larger will mean wasting
 * more memory, but fewer calls to realloc.  The default is 32; the
 * ideal value is a mystery for the ages.
 **********************************************************************/

#ifndef HAVE_LIBPTHREAD
# define HAVE_LIBPTHREAD 0
# undef THREADSAFE
# define THREADSAFE 0
#endif

#if THREADSAFE
#	include <pthread.h>
	static pthread_mutex_t irq_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
#	define pthread_mutex_lock(...)  (0)
#	define pthread_mutex_trylock(...) (0)
#	define pthread_mutex_unlock(...) (0)
#endif

#ifndef INTERRUPT_VECTOR_INCR
#	define INTERRUPT_VECTOR_INCR 32
#endif

/*******************************************************************//**
 * DOC: Vector Table
 * The vector table is a global variable for this module.
 *
 * We'll never shrink this table and never free it; it grows without size
 * as more interrupts are added.  This should never be a problem under any
 * reasonable circumstances; as adding and removing the same interrupts over
 * and over again will keep reusing the same memory.
 *
 * vector_table points to the start of the memory for the table, or NULL if the
 * table is of size 0.  vector_table_end points to the next address past the end
 * of the allocated vector table.
 *
 * Entries can be deleted from the table by setting their values to zero.  Since
 * both crate and irq are bitmasks, neither should be zero for a functional
 * entry.  All valid entries are kept contiguous when elements are deleted, so
 * the first entry with a zero is the last entry that actually exists;
 *
 * Macros foreach_vector and foreach_vector_st are defined to simplify iterating
 * the table.  foreach_vector will set @ptr to each element in the table, from
 * vector_table until reaching vector_table_end.  foreach_vector_st does the
 * same, but begins at an explicit @start rather than vector_table.
 **********************************************************************/

/**
 * struct table_t - Link a &struct v120irqd_selector to a client.
 * @selector:	A single multibit IRQ definition.
 * @data:		Information about the client requesting @selector.  @data will
 * 				never be 0 for a valid element.
 */
typedef struct table_t {
	struct v120irqd_selector selector;
	irqdata_t data;
} table_t;

static table_t * vector_table = NULL;
static table_t * vector_table_end = NULL;

/**
 * vector_table_allocated() - Return the total number of allocated vectors.
 *
 * The number of vectors used will be less than or equal to this number.
 */
static unsigned vector_table_allocated(void) {return vector_table_end - vector_table;}

/* Iterate over the table, from start until the last valid element. */
#define foreach_vector_st(ptr, start)	for (ptr=start; (ptr<vector_table_end) && (ptr->data!=0); ptr++)

/* Iterate over the entire vector table. */
#define foreach_vector(ptr)				foreach_vector_st(ptr, vector_table)

#ifdef USE_U64_HASH
	static bool hashmatch(const struct v120irqd_selector * sel, const struct v120irqd_selector * req)
	{
		uint64_t *sh, *rh;
		sh = (uint64_t *)&sel->crate;
		rh = (uint64_t *)&req->crate;
		return ((*sh & *rh) == *rh);
	}

	static bool hasheq(const struct v120irqd_selector *sel, const struct v120irqd_selector * req)
	{
		uint64_t *sh, *rh;
		sh = (uint64_t *)&sel->crate;
		rh = (uint64_t *)&req->crate;
		return (*sh == *rh);
	}
#else
	static bool hashmatch(const struct v120irqd_selector * sel, const struct v120irqd_selector * req)
	{
		return (((sel->crate & req->crate) == req->crate) &&
				((sel->irq & req->irq) == req->irq) &&
				((sel->vector & req->vector) == req->vector));
	}

	static bool hasheq(const struct v120irqd_selector *sel, const struct v120irqd_selector * req)
	{
		return	((sel->crate == req->crate) &&
				(sel->irq == req->irq) &&
				(sel->vector == req->vector));
	}
#endif

/**
 * locate_interrupt() - Find the table entry that corresponds to this request.
 * @request		The IRQ request.
 * @start		The table start address, usually vector_table.
 *
 * An entry is considered to be a match for this request if all bits in the
 * request crate and irq are set in the entry crate and irq, and the request
 * vector is either identically equal to the entry vectory, or the entry vector
 * is equal to ANYVECTOR.
 *
 * Warning: The mutex should already be locked when this function is called.
 *
 * Return: A pointer to the table entry, or NULL if no match.
 */
static table_t * locate_interrupt(const struct v120irqd_selector * request, table_t * const start)
{
	table_t *ptr;
	uint32_t pvec;
	foreach_vector_st(ptr, start) {
		/* All bits in the request are set in the entry if we've got a match. */
		if (!hashmatch(&ptr->selector, request)) continue;
		pvec = ptr->selector.vector;
		if ((pvec == ANYVECTOR) || (pvec == request->vector)) {
			return ptr;
		}
	}
	return NULL;
}

/**
 * remove_entry() - Move the entire list up one to replace victim.
 * @victim		The table entry to be deleted.
 *
 * This is slow, but it's not like this is an operation happening often. The
 * entry where the end of the list had been is zeroed out to mark it invalid.
 *
 * Warning: The mutex should already be locked when this function is called.
 */
static void remove_entry(table_t * victim)
{
	table_t *last;

	/* Find the last entry in the list.  Definitionally it's >= victim. */
	foreach_vector_st(last, victim) { ; }
	last--;

	/* Copy everything past the victim down one entry. */
	if (last != victim) {
		memmove(victim, victim+1, (last-victim)*sizeof(table_t));
	}

	/* Set the last entry (which has also been copied down one) to zero. */
	memset(last, 0, sizeof(table_t));
}

/* Look up an actual interrupt to determine who and how to return it. */
irqdata_t find_interrupt(struct v120irqd_selector * request)
{
	int e;
	irqdata_t ret;
	const table_t * ptr;

	if ((e = pthread_mutex_lock(&irq_mutex))) {
		logerror("failed pthread_mutex_lock: %s", strerror(e));
		errno = e;
		return 0;
	}

	ptr = locate_interrupt(request, vector_table);
	if (ptr == NULL) {
		errno = EINVAL;
		ret = 0;
	} else {
		ret = ptr->data;
		request->payload = ptr->selector.payload;
	}

	if ((e = pthread_mutex_unlock(&irq_mutex))) {
		logerror("failed pthread_mutex_unlock: %s", strerror(e));
		errno = e;
		ret = 0;
	}
	return ret;
}

/* Add one entry to the table, allocating more memory for the table if needed. */
int register_interrupt(irqdata_t sd, const struct v120irqd_selector * request)
{
	int e, ret;
	table_t *ptr;
	unsigned int old_count, new_count;

	logdebug("request for %04X:%02X:%08X",
		request->crate, request->irq, request->vector);

	if (sd == 0) {
		logerror("rejected NULL sd");
		return -EINVAL;
	}

	if (request->crate == 0) {
		logwarn("rejected illegal crate");
		return -EINVAL;
	}
	if (request->irq == 0 || (request->irq & (~ANYIRQ)) != 0) {
		logwarn("rejected illegal irq");
		return -EINVAL;
	}

	if ((e = pthread_mutex_lock(&irq_mutex))) {
		logerror("failed pthread_mutex_lock: %s", strerror(e));
		return -e;
	}

	/* Make sure that no one has already claimed this interrupt. */
	ptr = locate_interrupt(request, vector_table);
	if (ptr != NULL) {
		logerror("%04X:%02X:%08X already registered as %04X:%02X:%08X",
			request->crate, request->irq, request->vector,
			ptr->selector.crate, ptr->selector.irq, ptr->selector.vector
		);
		ret = -EADDRINUSE;
		goto freemutex;
	}

	/* Find the first unused entry in the table */
	if (vector_table == NULL) {
		assert(vector_table_end == NULL);
		ptr = NULL;
	} else {
		foreach_vector(ptr) { ; }
	}
	if (ptr == vector_table_end) {
		/* Looks like we're gonna need a bigger table. */
		old_count = vector_table_allocated();
		new_count = old_count + INTERRUPT_VECTOR_INCR;
		ptr = realloc(vector_table, new_count*sizeof(table_t));
		if (ptr == NULL) {
			logerror("couldn't increase table size: %s", strerror(errno));
			ret = -errno;
			goto freemutex;
		}

		memset(ptr + old_count, 0, INTERRUPT_VECTOR_INCR*sizeof(table_t));
		vector_table = ptr;
		vector_table_end = ptr + new_count;
		ptr = ptr + old_count;
	}

	/* Push this vector into the first unused entry. */
	ptr->data = sd;
	ptr->selector = *request;
	ret = 0;

freemutex:
	if ((e = pthread_mutex_unlock(&irq_mutex))) {
		logerror("failed pthread_mutex_unlock: %s", strerror(e));
		ret = -e;
	}
	return ret;
}

/* Remove one exact entry from the table. */
int release_interrupt(irqdata_t sd, const struct v120irqd_selector * request)
{
	int e;
	table_t *current, *ptr;
	int ret;

	logdebug("request for %04X:%02X:%08X",
		request->crate, request->irq, request->vector);

	if ((e = pthread_mutex_lock(&irq_mutex))) {
		logerror("failed pthread_mutex_lock: %s", strerror(e));
		return -e;
	}

	/* Find this exact request in the table. */
	current = NULL;
	foreach_vector(ptr) {
		if (hasheq(&ptr->selector, request)) {
			current = ptr;
			break;
		}
	}
	if (current == NULL) {
		logerror("%04X:%02X:%08X not registered",
			request->crate, request->irq, request->vector);
		ret = -EINVAL;
		goto freemutex;
	}
	if (current->data != sd) {
		logerror("%04X:%02X:%08X release request from wrong data",
			request->crate, request->irq, request->vector);
		ret = -EINVAL;
		goto freemutex;
	}

	remove_entry(current);
	ret = 0;

freemutex:
	if ((e = pthread_mutex_unlock(&irq_mutex))) {
		logerror("failed pthread_mutex_unlock: %s", strerror(e));
		ret = -e;
	}
	return ret;
}

/* Count the entries in the interrupt table. */
unsigned count_registered_interrupts() {
	int e;
	const table_t *end;
	int ret = 0;

	if ((e = pthread_mutex_lock(&irq_mutex))) {
		logerror("failed pthread_mutex_lock: %s", strerror(e));
		return -e;
	}

	foreach_vector(end) { ; }
	ret = end - vector_table;

	if ((e = pthread_mutex_unlock(&irq_mutex))) {
		logerror("failed pthread_mutex_unlock: %s", strerror(e));
		ret = -e;
	}
	return ret;
}

/* Wipe out all entries whose data is sd. */
int release_all_interrupts(irqdata_t sd)
{
	int e;
	table_t * ptr;

	if ((e = pthread_mutex_lock(&irq_mutex))) {
		logerror("failed pthread_mutex_lock: %s", strerror(e));
		return -e;
	}

	foreach_vector(ptr) {
		while (ptr->data == sd) {
			remove_entry(ptr);
		}
	}

	if ((e = pthread_mutex_unlock(&irq_mutex))) {
		logerror("failed pthread_mutex_unlock: %s", strerror(e));
		return -e;
	}
	return 0;
}

/* Clear the entire table. */
void free_all_interrupts(void)
{
	free(vector_table);
	vector_table = vector_table_end = NULL;
}

/* For each crate return a bitmask of all active IRQs. */
void list_registered_interrupts(uint8_t irqs[16]) {
	table_t *ptr;
	int crate;
	memset(irqs, 0, 16);
	foreach_vector(ptr) {
		for (crate = 0; crate < 16; crate++) {
			if (ptr->selector.crate & (1 << crate)) {
				irqs[crate] |= ptr->selector.irq;
			}
		}
	}
}
