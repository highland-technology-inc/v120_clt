===============================
V120 Interrupt Dispatch Service
===============================

Summary
=======

The V120 interrupt dispatch service is a userspace solution to sharing VME
interrupts across multiple applications.  Because each V120 interrupt endpoint
serves only one crate, and must be opened for exclusive access, a distributed
solution is necessary to allow the endpoints to be shared.

The dispatch service consists of a server and a client interface.  The server
creates a Unix socket connection at @/v120/v120irqd.  Clients connect
to this socket, request a list of interrupts that they are interested in, and
then await interrupt messages on the socket, either with a blocking read call
or with a select/poll/epoll mechanism.

Daemon
======

Introduction
------------

The daemon is sufficient to service all interrupts from as many crates as may
be on the system.  It is optimized primarily for readability of code, and as
such does not take full advantage of asynchronous I/O, multi-threading, or other
techniques that may slightly boost throughput.

With only one crate in the system, this is mostly irrelevant.  Since a given
VME crate can only process one transaction at a time, and the client process
should, upon receipt of the interrupt message, immediately service the interrupt
and respond, the VME crate would be busy the entire time anyhow, and therefore
increased attention to multitasking would be wasted.

With multiple crates, this choice could start to have a real impact; as multiple
crates really could be serviced simultaneously.  If this is a concern, users are
free to modify the daemon code to meet their performance needs.

Additionally, no authentication takes place, access control to the socket is
unrestricted.

Execution
---------

The daemon will run with user privileges, but if run with root privileges will
set its real-time priority to maximum and mlock all of its pages in order to
minimize latency.  Empirical testing results on this have not shown any
meaningful improvement, but it can't hurt.

There are several command line options available.

Logging
-------

The daemon can dump a fair amount of information to syslog, especially if started
with --debug.  To keep the main syslog file from being overrun, the file
``30-v120irqd.conf`` can be copied into the rsyslog configuration directory
to divert messages of lower severity than ERROR to /var/log/v120irqd.log.

Clients
=======

General Notes
-------------

Clients should ``#include <v120irqd.h>`` and link with ``-lv120irqd``.  This
makes the following functions available to them::
	
	int v120irqd_client(const char *socketname);
	int v120irqd_request(int socket, struct v120irqd_selector * sel);
	int v120irqd_getinterrupt(int socket, struct v120irqd_selector *sel);
	int v120irqd_ack(int socket);
	int v120irqd_nak(int socket);
	
	int v120irqd_release(int socket, struct v120irqd_selector * sel);
	int v120irqd_interrupt(int socket, struct v120irqd_selector *sel);
	int v120irqd_status(int socket, struct v120irqd_serverstatus *status);

Most clients will follow a very straightforward process.  At start time, call
``v120irqd_client`` to create a socket connection to the daemon, then make calls
to ``v120irqd_request`` to register for the interrupts the client will be interested
in, such as

	* Crate 3, IRQ7 if the vector address is 0xFFFF5678
	* Crate 0, any interrupt at any vector
	* Any crate, IRQ4 with any vector

Making these requests automatically sets the IRQ enable bits of the V120 in
each crate appropriately. 

Having done this the client will then use the blocking call v120irqd_getinterrupt
to get interrupts when the daemon catches them.  The easiest way to do this is
in a separate thread, but since communication is over a standard Unix socket
poll(), select(), or SIGIO trapping all work in the usual ways, and any are
viable options.

Having been notified of an interrupt, the client should take any necessary action
to release the interrupt (such as reading/writing a register on a RORA
interrupter), and then notify the daemon by calling v120irqd_ack().  v120irqd_nak() can
also be called to indicate an error, but this may result in the daemon disabling
this interrupt to protect the rest of the system.

This completes the client's obligations to the system, and it should go back to
waiting for the next interrupt.  If other actions need to be taken, the programmer
should ensure that the client is waiting for the next interrupt again by the time
it arrives, otherwise the entire interrupt system will stall waiting for it.

Interrupt Selection
-------------------

The struct v120irqd_selector has fields for crate, irq, and vector.  Crate 
and irq are both bitfields, allowing a single registration to encompass 
multiple selections, such as being registered for a device on any crate 
responding on IRQ5 with a vector of 0xFFFFC014.  For interrupters that are not
D32, the vector field should be padded with 1s for the data lines that will not
be driven, so for instance a D16 interrupter would always have a vector field of
0xFFFFxxxx, where xxxx is the actual vector.

The vector field takes the value ANYVECTOR (0xFFFFFFFF) as a special case to
admit any vector; the values ANYCRATE and ANYIRQ are simply bitmasks with all
the correct crates/IRQs selected.  So for instance, to register crate 2, IRQ7,
D32 vector 0x12345678 would be {.crate=0x0004, .irq=0x80, .vector=0x12345678}.
The same with a request for crates 2 or 0, irqs 7 or 1, would be
{.crate=0x0005, .irq=0x82, .vector=0x12345678}.  And the same for any crate
would be {.crate=0xFFFF, .irq=0x82, .vector=0x12345678}, and 0xFFFF is defined
as ANYCRATE for convenience.

The payload field of the v120irqd_selector allows a client to build a table, for
instance of function pointers, that the payload provides an index into upon
message receipt.  The server has already had to search through the interrupt
list in linear time; by providing a meaningful payload the client can be saved
from having to do the same.  The server has no use for the payload, and simply
returns it as originally provided for a given interrupt request.

Cleaning up
-----------
Closing the client program will close the socket connection.  Closing the socket
connection will cause the daemon to iterate through its internal list, removing
all the references to the now-vanished client.  This is a bit time-consuming, so
it shouldn't be done while there are other clients still needing to get their
interrupts with low latency.

Oddball Functions
-----------------

``v120irqd_release`` allows the client to manually unregister for a previously
registered interrupt.  Can't say I see much real-world need for it.

``v120irqd_interrupt`` is a message from the client to the daemon requesting an
interrupt be faked.  This is useful for testing other client applications.

``v120irqd_status`` requests status information about the daemon.

Rationale
=========

A userspace solution to interrupt dispatching is a bit non-traditional, making
the question of why we've done so rather than move all this code into the driver,
allow the endpoint to be opened by multiple processes, handle the registration
with ioctls on each open file handle.

The primary answer is fault-tolerance. If client code fails to acknowledge the
clearing of the interrupt, or an interrupt occurs that no one holds the vector
for, or any number of things, the interrupt logic could be completely locked up.
This is unavoidable; handling VME interrupts in a generic way *requires* that it
be fully handshaked with user logic.  The only other alternative would be to
expect users to write kernel-mode drivers for each VME card they want to
receive interrupts on, and no one wants to do that.  This way, the worst thing
that can happen is that the daemon locks up, and can be killed.  If it were
being handled at the driver level, user code could lock up the entire driver and
possibly cause a kernel panic.

Also, the daemon is meant to be modified if the application demands it, or can
even be taken out of the process entirely and interrupt dispatching handled
entirely by the client application.  This is substantially more onerous if the
dispatching logic is already baked into the driver.  User space code is easily
modified, and easily iterated on until the behavior is correct.  Kernel code
comes with a much heavier lift, both practically and psychologically.

The performance impact is hard to quantify.  On our test system we found that
the interrupt latency, hardware->V120->driver->daemon->client, was regularly
over 200 us and sometimes as much as 350 us when the interrupts were occurring
infrequently, but dropped to only about 15 us (yes that's over 20:1) when the
interrupts occurred frequently.

By taking the daemon out of the process and having the client communicate with
the driver directly, thus also removing all dispatch logic, the long delays were
up around 130 us but the fast ones were still only about 14 us.  These
distributions have very long right tails, so it's hard to say much definitively,
but it looks like the only circumstance in which the daemon is a real performance
hit is the one in which you need low-latency interrupts that happen infrequently
(numbers on the order of 1 per second).  And this is all clearly tied up with
the caching effects of the system, which will vary from system to system, so
when all is said and done there's not much to say and little to do.

Licensing
=========

The V120 Interrupt Dispatch Service, and all code, documentation, and examples, 
are all licensed under the 3-clause Modified BSD License, the entirety of which
is provided as license.txt.  This is an extremely permissive license, intended
to formalize your right to do what you wish how you wish whenever you wish with
this project.

Specifically, this is not GPL code, and carries no copyleft restrictions forcing
you to distribute modifications and improvements that you may make.  That said,
anything that you'd like to contribute back upstream we would of course be
grateful for.  We've never had any ability to predict the places our customers
will take our products, and we love to hear about what tricks you've managed to
teach them.

Thanks for reading this far, now go out and do something amazing.

Rob Gaddi, Highland Technology
23-Jun-2015
