/*
 * A sample client that registers with the v120-interrupt-dispatcher for a
 * selection of interrupts.
 *
 * This software is released under the Modified BSD License, and may be
 * redistributed according to the terms stated in license.txt, which must
 * be kept with this file.
 *
 * Rob Gaddi, Highland Technology.  22-Jun-2015
 */

#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v120irqd.h"
#include "config.h"

struct irq_request {
	int crate, irq;
	uint32_t vector;
	char * msg;
};
#define BIT(n) (1 << (n))

/**********************************************************************
 * Possible interrupt sets to register for.
 **********************************************************************/

static struct irq_request *irq_request_options[] = {
	(struct irq_request[]){
		{BIT(0), BIT(1), 0xDEADBEEF,	"C0 dead beef"},
		{BIT(0), BIT(1), 0xFEEDD00D,	"C0 feed the man"},
		{BIT(0), BIT(1), ANYVECTOR,		"C0 IRQ1 remaining"},
		{BIT(0), BIT(3), ANYVECTOR,		"C0 IRQ3"},
		{0, 0, 0}
	},
	(struct irq_request[]){
		{ANYCRATE, BIT(7), ANYVECTOR,	"Any IRQ7"},
		{0, 0, 0}
	},
	(struct irq_request[]){
		{BIT(1), BIT(1), 0xDEADBEEF,	"C1 deader beef"},
		{BIT(1), BIT(1), 0xFEEDD00D,	"C1 hungrier man"},
		{BIT(1), BIT(3), ANYVECTOR,		"C1 IRQ3"},
		{BIT(1), BIT(4), ANYVECTOR,		"C1 IRQ4"},
		{0, 0, 0}
	}
};
#define NCLIENTS (sizeof(irq_request_options)/sizeof(*irq_request_options))

/**********************************************************************
 * Command line argument parsing.
 **********************************************************************/

const char* argp_program_version =
"Dummy V120 Interrupt Client " VERSION "\n"
"Copyright (C) 2015 Highland Technology, Inc.\n"
"Released under the 3-Clause BSD license <http://opensource.org/licenses/BSD-3-Clause>.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law. "
;

static int clientselection = -1;
static int parse_opt(int key, char *arg, struct argp_state *state)
{
	int client;
	char * endptr;

	switch (key) {
	case ARGP_KEY_ARG:
		client = strtoul(arg, &endptr, 10);
		if (client >= NCLIENTS) {
			argp_failure(state, 1, 0, "Client selection %d must be in the range 0-%lu", client, NCLIENTS-1);
		}
		clientselection = client;
		break;

	case ARGP_KEY_END:
		if (clientselection < 0) {
			argp_usage(state);
		}
	}
	return 0;
}

/**********************************************************************
 * Main application code.
 **********************************************************************/

int main(int argc, char * argv[])
{
	int sock, err;

	/* Parse the command line arguments. */
	struct argp_option options[] = {
		{0}
	};
	char args_doc[64];
	snprintf(args_doc, sizeof(args_doc), "0-%lu", NCLIENTS-1);

	struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.args_doc = args_doc,
	};
	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err) {
		fprintf(stderr, "Argument parsing failed: %s", strerror(err));
		return 1;
	}

	/* Open a socket to the server. */
	printf("Opening socket.\n");
	sock = v120irqd_client(NULL);
	if (sock < 1) {
		perror("Failed opening client socket");
		return 1;
	}

	/* Configure our IRQ requests. */
	const struct irq_request * const requests = irq_request_options[clientselection];
	const struct irq_request *ptr;
	struct v120irqd_selector st;
	int payload = 0;

	printf("Requesting IRQs.\n");
	for (ptr = requests; ptr->irq != 0; ptr++) {
		st.crate = ptr->crate;
		st.irq = ptr->irq;
		st.vector = ptr->vector;
		st.payload = payload;

		if (v120irqd_request(sock, &st)) {
			perror("Error requesting interrupt.");
			return 1;
		}
		payload++;
	}
	printf("Successfully requested %d interrupt(s)\n", payload);

	/* Now run our infinite loop waiting for IRQs.  In a real application, this
	 * is probably a background thread that signals the main application when
	 * activity happens.
	 */
	while (1) {
		printf("Waiting for interrupt...\n");

		memset(&st, 0, sizeof(st));
		err = v120irqd_getinterrupt(sock, &st);
		if (err < 0) {
			fprintf(stderr, "Failed getting interrupt: %s", strerror(-err));
			return 1;
		}
		err = v120irqd_ack(sock);
		if (err < 0) {
			fprintf(stderr, "Failed acknowledging IRQ: %s", strerror(-err));
			return 1;
		}

		printf("Client %d receieved interrupt:\n", clientselection);
		printf("    crate:   %d\n", v120irqd_ilog2f(st.crate));
		printf("    irq:     %d\n", v120irqd_ilog2f(st.irq));
		printf("    vector:  %08X\n", st.vector);
		printf("    payload: %08X\n", st.payload);
		printf("    message: %s\n", requests[st.payload].msg);
	}
	return 0;
}
