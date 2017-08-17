/*
 * Unit tests for some of the low-level compilation things.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <stddef.h>

#include "v120irqd.h"
#include "unity/unity.h"

/* Confirm that the hash member of the structure correctly encompasses the
 * entirety of the crate, irq, and vector.
 */
 
#define voff(member) offsetof(struct v120irqd_selector, member)
 
void test_irq_hashsize(void)
{
	struct v120irqd_selector st;
	
	size_t start = voff(crate);
	size_t end = voff(vector) + sizeof(st.vector);
	
	if (start > voff(irq)) {
		start = voff(irq);
	} else  {
		end = voff(irq) + sizeof(st.irq);
	}
	
	if (start > voff(vector)) {
		start = voff(irq);
	} else if (end <= voff(vector)) { 
		end = voff(vector) + sizeof(st.vector);
	}
	
	TEST_ASSERT(voff(irq) > voff(crate));
	TEST_ASSERT(voff(irq) < voff(vector));
	TEST_ASSERT_EQUAL(end - start, sizeof(uint64_t));
}

/* Confirm the v120irqd_ilog2f function on some randomly generated test cases. */
void test_ilog2f(void)
{
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(1133549021), 30);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(2776), 11);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(20), 4);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(311437), 18);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(1042257788), 29);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(37025), 15);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(133777), 17);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(10850), 13);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(10065178), 23);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(4), 2);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(18925048), 24);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(16469105), 23);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(3340), 11);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(41174117), 25);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(2028883), 20);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(16637662), 23);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(551203), 19);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(423321454), 28);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(47732), 15);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(1197570), 20);
	TEST_ASSERT_EQUAL(v120irqd_ilog2f(0), -1);
}

int main(void) {
	UnityBegin(__FILE__);
	RUN_TEST(test_irq_hashsize);
	RUN_TEST(test_ilog2f);
	return UnityEnd();
}
