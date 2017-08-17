/*
 * A sample client that requests interrupts for the V680, opening the interrupt
 * endpoint directly rather than using v120irqd.
 * 
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 * 
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <argp.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>

#include <V120.h>

static bool EARLY_WAKE=false;
static bool USE_REALTIME=false;

struct v680_registers {
	const uint16_t vxi_mfr;
	const uint16_t vxi_type;
	const uint16_t vxi_sts;
	uint16_t vector;
	
	uint16_t control;
	const uint16_t hit;
	const uint16_t dblhit;
	uint16_t irqmask;
	
	uint16_t resets;
	uint16_t select;
	
	const uint16_t t0;
	const uint16_t t1;
	const uint16_t t2;
	
	const uint16_t _dummy[19];
};

#define BIT(x) (1 << (x))

#define V68O_CTL_GATE		BIT(0)
#define V68O_CTL_FGATE		BIT(1)
#define V68O_CTL_POS		BIT(2)
#define V68O_CTL_IRQFLG		BIT(3)
#define V68O_CTL_SYNC		BIT(4)
#define V68O_CTL_ET8		BIT(5)
#define V68O_CTL_GTEST		BIT(6)
#define V68O_CTL_TTEST		BIT(7)
#define V68O_CTL_GSTAT		BIT(9)

#define HIT(x) (BIT(x))
#define GATEFLAG BIT(9)

V120_HANDLE *crate = NULL;
volatile struct v680_registers *DUT;

/**
 * Spin on interrupts until the program exits.
 */
static void *process_interrupts(void *data)
{
	pthread_t mainthread = *(pthread_t *)data;
	
	
	if (USE_REALTIME) {
		const int policy = SCHED_FIFO;
		struct sched_param sp;
		sp.sched_priority = sched_get_priority_max(policy);
		if (sched_setscheduler(0, policy, &sp)) {
			perror("sched_setscheduler");
			exit(1);
		}
	}
	
	/* Request interrupt notification from the V120. */
	int fd = v120_irq_open(crate);
	V120_IRQ *irq = v120_get_irq(crate);
	irq->iackcfg = 0xFFFFFFFF;
	irq->irqen = BIT(7);
	
	/* Infinite loop catching interrupts. */
	while (true) {
		/* Block until we get an interrupt. */
		if (read(fd, NULL, 1) == 0) {
			printf("IRQ endpoint closed.");
			exit(1);
		}
		
		/* Confirm it's ours.  Even this could be skipped if we needed
		 * performance.
		 */
		unsigned irqstatus = irq->irqstatus;
		if ((irqstatus & BIT(7)) == 0) {
			printf("IRQ 0x%02X not 7", irqstatus);
			exit(1);
		}
		
		uint32_t vector = irq->iack_vector[7];
		if (vector != 0xFFFF008E) {
			printf("Vector %08X not 0xFFFF008E", vector);
			exit(1);
		}
		
		/* Handle the RORA interrupt. */
		DUT->resets = GATEFLAG;
		
		printf("Got interrupt, IRQ 7, vector %04X\n", vector);
		if (EARLY_WAKE) pthread_kill(mainthread, SIGUSR1);
	}
}

static void wakeup(int signal) {}

#define FAST 256
#define MLOCK 257
#define RT 258

static int parse_opt(int key, char *arg, struct argp_state *state)
{
	switch (key) {
		case FAST: 
			EARLY_WAKE = true;
			break;
		case MLOCK: 
			if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
				perror("mlockall");
				exit(1);
			}
			break;
		case RT: 
			USE_REALTIME = true;
			break;
	}
	return 0;
}

static int parseArgs(int argc, char * argv[])
{
	/* Command line argument parsing. */
	struct argp_option options[] = {
		{"fast",	FAST,	NULL,	0,	"Do the next interrupt immediately on ACK rather than wait the full second"},
		{"mlock",	MLOCK,	NULL,	0,	"mlockall the pages"},
		{"rt",		RT,		NULL,	0,	"Set maximum realtime priority."},
		{0}
	};
	struct argp argp = {
		.options = options,
		.parser = parse_opt
	};
	return argp_parse(&argp, argc, argv, 0, NULL, NULL);
}

int main(int argc, char* argv[])
{
	if (parseArgs(argc, argv)) {
		return 1;
	}
	
	/* Sanity checks */
	assert(sizeof(struct v680_registers) == 64);
	
	/* Open the device */
	crate = v120_open(0);
	if (crate == NULL) {
		perror("v120_open()");
		return 1;
	}
	
	VME_REGION region = {
		.vme_addr = 0xE000,
		.len = sizeof(struct v680_registers),
		.config = V120_A16 | V120_ESHORT | V120_SMAX | V120_D16,
		.tag = "V680 DUT"
	};
	if (v120_add_vme_region(crate, &region) == NULL) {
		perror("v120_add_vme_region()");
		return 1;
	}
	v120_allocate_vme(crate, 0);
	
	/* Confirm we've got our device. */
	DUT = region.base;
	assert(DUT->vxi_mfr == 0xFEEE);
	assert(DUT->vxi_type == 22680);

	/* Start our interrupt listener thread. */
	pthread_t interrupt_thread, self;
	self = pthread_self();
	int err = pthread_create(&interrupt_thread, NULL, &process_interrupts, &self);
	if (err) {
		errno = -err;
		perror("pthread_create()");
		return 1;
	}

	/* Configure the interrupt vector.  Only the low 8 bits are used. */
	DUT->vector = 0x8E;
	DUT->irqmask = GATEFLAG;
	
	/* Register a wakeup handler so the thread can get our attention. */
	struct sigaction sa = {
		.sa_handler = &wakeup,
	};
	sigaction(SIGUSR1, &sa, NULL);
	
	/* Cause interrupts for 10 seconds. */
	time_t endtime = time(NULL) + 10;
	while (time(NULL) < endtime) {
		DUT->control = 0;
		DUT->control = V68O_CTL_FGATE;
		DUT->control = 0;
		
		sleep(1);
	}
	printf("Done.\n");
	return 0;
}
