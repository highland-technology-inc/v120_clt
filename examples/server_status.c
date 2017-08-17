/*
 * A trivial program to report the v120irqd status.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 * 
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v120irqd.h"

int main(void) {
	int sock; 
	
	/* Open a socket to the server. */
	printf("Opening socket.\n");
	sock = v120irqd_client(NULL);
	if (sock < 1) {
		perror("Failed opening client socket");
		return 1;
	}
	
	/* Get the server information. */
	struct v120irqd_serverstatus status;	
	if (v120irqd_status(sock, &status)) {
		perror("Error requesting status.");
		return 1;
	}

	printf("PID:        %u\n", status.pid);
	printf("Crates:     ");
	if (status.crates == 0) {
		printf("[]\n");
	} else {
		bool anyyet = false;
		printf("[");
		for (int idx=0; idx<16; idx++) {
			if (status.crates & (1 << idx)) {
				if (anyyet) printf(", %d", idx);
				else 		printf("%d", idx);
				anyyet = true;
			}
		}
		printf("]\n");
	}
	printf("Clients:    %u\n", status.clients - 1);
	printf("Interrupts: %u\n", status.irq_requests);
	return 0;
}
