/*
 * Unit tests for irq_vector_table.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include "v120irqd.h"
#include "irq_vector_table.h"

#include "unity/unity.h"

#define TEST_NOFAIL(x)									\
	do {												\
		int _le = (x);									\
		TEST_ASSERT_FALSE_MESSAGE(_le, strerror(-_le));	\
	} while (0)

static int easy_register_interrupt(
	irqdata_t sd,
	uint16_t crate, uint16_t irq, uint32_t vector,
	uint32_t payload)
{
	struct v120irqd_selector st;
	st.crate = (crate == ANYCRATE) ? ANYCRATE : (1 << crate);
	st.irq   = (irq == ANYIRQ) ? ANYIRQ : (1 << irq);
	st.vector = vector;
	st.payload = payload;
	return register_interrupt(sd, &st);
}


void setUp(void) {
	/* Pretend we have socket descriptors 1 and 2.
	 * Set up specific requests on crates 1 and 2.
	 */

	/*						sd		crate	irq		vec,			payload */
	easy_register_interrupt(1, 		1,		3,		0xFFFFFFDE,		1);
	easy_register_interrupt(2, 		2,		3,		0xFFFFFFDE,		2);
	easy_register_interrupt(1, 		2,		7,		0xDEADBEEF,		3);
	easy_register_interrupt(2, 		1,		7,		0xDEADBEEF,		4);
	easy_register_interrupt(1, 		1,		3,		0xFFFF0000,		5);
	easy_register_interrupt(2,		2,		3,		0xFFFFABBA,		6);

	/* Now set up a couple of generic fallbacks.  We will specifically
	 * not trap ANYCRATE/ANYIRQ/ANYVECTOR to make sure we can fall
	 * through that test. */
	easy_register_interrupt(1,	ANYCRATE,	4,		ANYVECTOR,		7);
	easy_register_interrupt(2,		2,		ANYIRQ,	ANYVECTOR,		8);
}

void tearDown(void) {
	free_all_interrupts();
}

struct testsuite_t {
	int socket, crate, irq, vec, payload;
};

/** Run count tests from a testsuite array. */
void _run_testsuite(const struct testsuite_t *suite, size_t count, unsigned sourceln)
{
	const struct testsuite_t *ptr;
	struct v120irqd_selector st;
	int sd;
	unsigned testidx;
	char msg[128];

	for (testidx=0; testidx<count; testidx++) {
		ptr = suite + testidx;
		snprintf(msg, sizeof(msg),"Test #%d", testidx);

		st.crate = (1 << ptr->crate);
		st.irq = (1 << ptr->irq);
		st.vector = ptr->vec;
		sd = find_interrupt(&st);

		UNITY_TEST_ASSERT_EQUAL_INT(ptr->socket, sd, sourceln, msg);
		if (sd) {
			UNITY_TEST_ASSERT_EQUAL_HEX32(ptr->payload, st.payload, sourceln, msg);
		}
	}
}
#define run_testsuite_n(suite, count) _run_testsuite((suite), (count), __LINE__)
#define run_testsuite(suite) run_testsuite_n((suite), (sizeof(suite)/sizeof(*suite)))

/** This testsuite corresponds to the setUp state. */
static const struct testsuite_t default_suite[] = {
	//sock		crate	irq		vec				payload
	{1,			5,		4,		0xF00DD00D,		7},	//ANYCRATE ANYVECTOR
	{2, 		2,		3,		0xFFFFFFDE,		2},
	{1, 		1,		3,		0xFFFF0000,		5},
	{0,         5,      7,      0x87654321,		0},	//No match
	{2,			2,		1,		0x12345678,		8},	//ANYIRQ ANYVECTOR
	{2,			2,		3,		0xFFFFABBA,		6},
	{1, 		1,		3,		0xFFFFFFDE,		1},
	{0,         1,      2,      0x87654321,		0},	//No match
	{1, 		2,		7,		0xDEADBEEF,		3},
	{2, 		1,		7,		0xDEADBEEF,		4}
};

void test_setup(void) {
	TEST_ASSERT_EQUAL(8, count_registered_interrupts());
	uint8_t irqs[16];
	uint8_t expected[16] = {
		0x10, 0x98, 0xFE, 0x10,
		0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x10, 0x10
	};
	list_registered_interrupts(irqs);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, irqs, 16);
}

void test_addchecking(void) {
	int ret;
	syslog(LOG_ERR, "Expecting an error on the following line:");
	ret = easy_register_interrupt(1, 3, 4, 0xDEADBEEF, 115);
	TEST_ASSERT_EQUAL(-EADDRINUSE, ret);
}

void test_resize(void) {
	int i;
	const int IRQS = 4;
	struct v120irqd_selector st[IRQS];
	struct v120irqd_selector * ptr;

	unsigned int total_irqs;

	int sd;

	for (i=0; i<IRQS; i++) {
		ptr = &st[i];
		ptr->crate = 3;
		ptr->irq = ANYIRQ;
		ptr->vector = (0xFFFF0000) | (i * 4);
		ptr->payload = 100 + i;
		TEST_NOFAIL(register_interrupt(1, ptr));
	}

	/* We should have successfully enlarged the hell out of the table. */
	total_irqs = IRQS + 8;
	TEST_ASSERT_EQUAL(total_irqs, count_registered_interrupts());

	/* Test them all to make sure that we've got all the new ones and
	 * all the default ones.  If the table has the right count, and has
	 * all of the correct interrupts, then it must be right. */
	for (i=0; i<IRQS; i++) {
		ptr = &st[i];
		ptr->payload = 0;
		sd = find_interrupt(ptr);
		TEST_ASSERT_EQUAL(1, sd);
		TEST_ASSERT_EQUAL(100+i, ptr->payload);
	}
	run_testsuite(default_suite);

	/* Delete all the newly created interrupts out of order. */
	for (i=IRQS/2; i<IRQS; i++) {
		ptr = &st[i];
		TEST_NOFAIL(release_interrupt(1, ptr));
		total_irqs--;
		TEST_ASSERT_EQUAL(total_irqs, count_registered_interrupts());
	}
	for (i=0; i<IRQS/2; i++) {
		ptr = &st[i];
		TEST_NOFAIL(release_interrupt(1, ptr));
		total_irqs--;
		TEST_ASSERT_EQUAL(total_irqs, count_registered_interrupts());
	}
}

/* Delete one interrupt from the list. */
void test_simpledelete(void) {
	const int socket = 2;
	struct v120irqd_selector st = {
		.crate = (1 << 1),
		.irq = (1 << 7),
		.vector = 0xDEADBEEF
	};
	release_interrupt(socket, &st);
	TEST_ASSERT_EQUAL(7, count_registered_interrupts());
}

/* Confirm that we are looking interrupts up successfully. */
void test_simplelookup(void) {
	run_testsuite(default_suite);
}

int main(void) {
	openlog(NULL, LOG_PERROR, 0);
	setlogmask(LOG_UPTO(LOG_NOTICE));
	syslog(LOG_DEBUG, "Logging is active.");
	UnityBegin(__FILE__);
	RUN_TEST(test_setup);
	RUN_TEST(test_simpledelete);
	RUN_TEST(test_simplelookup);
	RUN_TEST(test_addchecking);

	RUN_TEST(test_resize);
	return UnityEnd();
}
