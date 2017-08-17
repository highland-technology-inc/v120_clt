/*
 * A simple program to send fake IRQ information to v120irqd.
 *
 * The daemon should then send it on to the registered clients as if it came
 * from VME.  Useful for testing early stage interrupt handling issues.
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

/**********************************************************************
 * Command line argument parsing.
 **********************************************************************/

const char* argp_program_version =
"V120 Fake Interrupt Generator " VERSION "\n"
"Copyright (C) 2015 Highland Technology, Inc.\n"
"Released under the 3-Clause BSD license <http://opensource.org/licenses/BSD-3-Clause>.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law. "
;

static struct v120irqd_selector fake_irq_info;

/* Set the crate if legal or die. */
static void set_crate(unsigned int crate, struct argp_state *state)
{
	if (fake_irq_info.crate != 0) {
		argp_failure(state, 1, 0, "Multiple attempts to set crate.");
	}
	if (crate >= 16) {
		argp_failure(state, 1, 0, "Crate must be in range 0-15, was %d\n", crate);
	}
	fake_irq_info.crate = (1 << crate);
}

/* Set the irq if legal or die. */
static void set_irq(unsigned int irq, struct argp_state *state)
{
	if (fake_irq_info.irq != 0) {
		argp_failure(state, 1, 0, "Multiple attempts to set irq.");
	}
	if ((irq < 1) || (irq > 7)) {
		argp_failure(state, 1, 0, "Irq must be in range 1-7, was %d\n", irq);
	}
	fake_irq_info.irq = (1 << irq);
}

/* Set the vector if legal or die. */
static void set_vector(uint32_t vector, struct argp_state *state)
{
	if (fake_irq_info.vector != 0) {
		argp_failure(state, 1, 0, "Multiple attempts to set vector.");
	}
	fake_irq_info.vector = vector;
}

static int parse_opt(int key, char *arg, struct argp_state *state)
{
	unsigned int crate, irq;
	uint32_t vector;
	char * endptr;

	switch (key) {
	case ARGP_KEY_ARG:
		if (sscanf(arg, "%u:%u:%x", &crate, &irq, &vector) != 3) {
			argp_failure(state, 1, 0, "Positional argument must be in the form CRATE:IRQ:VECTOR.");
		}
		set_crate(crate, state);
		set_irq(irq, state);
		set_vector(vector, state);
		break;

	case 'c':
		if (*arg == '0')	crate = strtoul(arg, &endptr, 10);
		else				crate = strtoul(arg, &endptr, 0);
		if (*endptr != '\0') {
			argp_failure(state, 1, 0, "Nonnumeric character in CRATE.");
		}
		set_crate(crate, state);
		break;

	case 'i':
		if (*arg == '0')	irq = strtoul(arg, &endptr, 10);
		else				irq = strtoul(arg, &endptr, 0);
		if (*endptr != '\0') {
			argp_failure(state, 1, 0, "Nonnumeric character in IRQ.");
		}
		set_irq(irq, state);
		break;

	case 'v':
		vector = strtoul(arg, &endptr, 16);
		if (*endptr != '\0') {
			argp_failure(state, 1, 0, "Non-hexadecimal character in VECTOR.");
		}
		set_vector(vector, state);
		break;

	case ARGP_KEY_END:
		if (fake_irq_info.crate == 0 || fake_irq_info.irq == 0) {
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
		{"crate",	'c',	"CRATE",	0,	"Crate number, 0-15"},
		{"irq",		'i',	"IRQ",		0,	"VME IRQ number, 1-7"},
		{"vector",	'v',	"VECTOR",	0,	"IRQ vector in hexadecimal"},
		{0}
	};
	struct argp argp = {
		.options = options,
		.parser = parse_opt,
		.args_doc = "[CRATE:IRQ:VECTOR]",
		.doc = "Only one request can be provided; either as individual options or as a CRATE:IRQ:VECTOR triple."
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

	/* Request the IRQ. */
	printf("Requesting fake IRQ... ");
	err = v120irqd_interrupt(sock, &fake_irq_info);
	if (err) {
		printf("failed: %s\n", strerror(-err));
		return 1;
	}
	printf("success\n");
	return 0;
}
