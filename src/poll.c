/*
 * atheme-services: A collection of minimalist IRC services
 * poll.c: poll(2) socket engine.
 *
 * Copyright (c) 2005-2007 Atheme Project (http://www.atheme.org)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/poll.h>

#include "atheme.h"
#include "internal.h"
#include "datastream.h"

/*
 * linux does not provide POLLWRNORM by default, and we're not _XOPEN_SOURCE.
 * so.... we have to do this crap below.
 */
#ifndef POLLRDNORM
#define POLLRDNORM POLLIN
#endif
#ifndef POLLWRNORM
#define POLLWRNORM POLLOUT
#endif

struct pollfd pollfds[FD_SETSIZE]; /* XXX We need a define indicating MAXCONN. */

/*
 * init_socket_queues()
 *
 * inputs:
 *       none
 *
 * outputs:
 *       none
 *
 * side effects:
 *       when using select, we don't need to do anything here.
 */
void init_socket_queues(void)
{
	memset(&pollfds, 0, sizeof(struct pollfd) * FD_SETSIZE);
}

/*
 * update_poll_fds()
 *
 * inputs:
 *       none
 *
 * outputs:
 *       none
 *
 * side effects:
 *       registered sockets are prepared for the poll() loop.
 */
static void update_poll_fds(void)
{
	node_t *n;
	connection_t *cptr;
	int slot = 0;

	LIST_FOREACH(n, connection_list.head)
	{
		cptr = n->data;

		cptr->pollslot = slot;

		if (CF_IS_CONNECTING(cptr) || sendq_nonempty(cptr))
		{
			pollfds[slot].fd = cptr->fd;
			pollfds[slot].events |= POLLWRNORM;
			pollfds[slot].revents = 0;
		}
		if (cptr->read_handler)
		{
			pollfds[slot].fd = cptr->fd;
			pollfds[slot].events |= POLLRDNORM;
			pollfds[slot].revents = 0;
		}
		slot++;
	}
}

/*
 * connection_select()
 *
 * inputs:
 *       delay in milliseconds
 *
 * outputs:
 *       none
 *
 * side effects:
 *       registered sockets and their associated handlers are acted on.
 */
void connection_select(int delay)
{
	int sr;
	node_t *n, *tn;
	connection_t *cptr;
	int slot;

	update_poll_fds();

	if ((sr = poll(pollfds, connection_list.count, delay)) > 0)
	{
		/* Iterate twice, so we don't touch freed memory if
		 * a connection is closed.
		 * -- jilles */
		LIST_FOREACH_SAFE(n, tn, connection_list.head)
		{
			cptr = n->data;
			slot = cptr->pollslot;

			if (pollfds[slot].revents == 0)
				continue;

			if (pollfds[slot].revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR))
			{
				pollfds[slot].events &= ~(POLLRDNORM | POLLIN | POLLHUP | POLLERR);
				cptr->read_handler(cptr);
			}
		}

		LIST_FOREACH_SAFE(n, tn, connection_list.head)
		{
			cptr = n->data;
			slot = cptr->pollslot;

			if (pollfds[slot].revents == 0)
				continue;
			if (pollfds[slot].revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR))
			{
				pollfds[slot].events &= ~(POLLWRNORM | POLLOUT | POLLHUP | POLLERR);
				cptr->write_handler(cptr);
			}
		}

		LIST_FOREACH_SAFE(n, tn, connection_list.head)
		{
			cptr = n->data;
			if (cptr->flags & CF_DEAD)
				connection_close(cptr);
		}
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
