/*
 * A trivial program to cause a fake IRQ on V120 #0 and catch it using v120irqd.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <V120.h>
#include "v120irqd.h"

#define BIT(x)	(1 << (x))

static V120_HANDLE *hVME;
static V120_IRQ* irq;

static void make_fakeirq(void)
{
	/* Confirm that the server set the IRQEN properly */
	uint32_t irqen = irq->irqen;
	printf("IRQEN = 0x%08X\n", irqen);

	/* Fire a fake IRQ3. */
	irq->irqen |= BIT(3 + 8);

	printf("Generated interrupt.\n");
}

static int wait_fakeirq(void)
{
	/* Open a socket to the server. */
	printf("Opening socket.\n");
	int sock = v120irqd_client(NULL);
	if (sock < 1) {
		perror("Failed opening client socket");
		return 1;
	}

	/* Request IRQ3. */
	struct v120irqd_selector irq_request = {
		.crate = BIT(0), .irq = BIT(3), .vector=ANYVECTOR, .payload=0
	};
	if (v120irqd_request(sock, &irq_request)) {
		perror("Error requesting interrupt.");
		return 1;
	}

	/* Block until it comes in. */
	printf("Waiting on interrupt.\n");
	if (v120irqd_getinterrupt(sock, &irq_request)) {
		perror("Error requesting interrupt.");
		return 1;
	}

	printf("Receieved interrupt:\n");
	printf("    crate:   %d\n", v120irqd_ilog2f(irq_request.crate));
	printf("    irq:     %d\n", v120irqd_ilog2f(irq_request.irq));
	printf("    vector:  %08X\n", irq_request.vector);
	printf("    payload: %08X\n", irq_request.payload);

	/* Clear the interrupt. */
	irq->irqen &= ~BIT(3 + 8);
	if (v120irqd_ack(sock)) {
		perror("Error ACKing interrupt.");
		return 1;
	}
	return 0;
}

int main(void)
{
	/* Connect to the crate. */
	hVME = v120_open(0);
	irq = v120_get_irq(hVME);

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork() failed");
		return 1;
	} else if (pid == 0) {
		/* Let the child process wait on the fake interrupt. */
		return wait_fakeirq();
	} else {
		/* The parent process should generate the fake interrupt. */
		sleep(1);
		make_fakeirq();
		waitpid(pid, NULL, 0);
		printf("Child process complete; exiting\n");
		return 0;
	}
}
