/* Copyright (C) 2009 - Virtualsquare Team
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <vde3.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <event.h>

/*
 * vde_event_handler which uses libevent as a backend, handling of recurrent
 * timeouts (aka periodic) is implemented as well.
 *
 * usage can be like something this:
 *
 * event_init();
 * ...
 * vde_context_init(ctx, &libevent_eh);
 * ...
 * event_dispatch();
 *
 */

// recurring timeout handling
struct rtimeout {
  struct event *ev;
  struct timeval *timeout;
  vde_event_cb cb;
  void *arg;
};

void rtimeout_cb(int fd, short events, void *arg)
{
  struct rtimeout *rt = (struct rtimeout *)arg;

  // call user callback and reschedule the timeout
  rt->cb(fd, events, rt->arg);

  // XXX(godog): this breaks if timeout_del is called inside the cb
  timeout_add(rt->ev, rt->timeout);
}

// XXX check if libevent has been initialized?
void *libevent_event_add(int fd, short events, const struct timeval *timeout,
                         vde_event_cb cb, void *arg)
{
  struct event *ev;

  ev = (struct event *)malloc(sizeof(struct event));
  if (!ev) {
    vde_error("%s: can't allocate memory for new event", __PRETTY_FUNCTION__);
    errno = ENOMEM;
    return NULL;
  }

  event_set(ev, fd, events, cb, arg);
  event_add(ev, timeout);

  return ev;
}

void libevent_event_del(void *event)
{
  struct event *ev = (struct event *)event;

  event_del(ev);
  free(ev);
}

void *libevent_timeout_add(const struct timeval *timeout, short events,
                           vde_event_cb cb, void *arg)
{
  struct event *ev;
  struct rtimeout *rt;

  ev = (struct event *)malloc(sizeof(struct event));
  if (!ev) {
    vde_error("%s: can't allocate memory for timeout", __PRETTY_FUNCTION__);
    errno = ENOMEM;
    return NULL;
  }

  rt = (struct rtimeout *)calloc(1, sizeof(struct rtimeout));
  if (!rt) {
    if (ev) {
      free(ev);
    }
    vde_error("%s: can't allocate memory for timeout", __PRETTY_FUNCTION__);
    errno = ENOMEM;
    return NULL;
  }
  rt->ev = ev;

  // if it is a recurrent timeout hook our callback instead of user's
  if (events & VDE_EV_PERSIST) {
    rt->timeout = (struct timeval *)malloc(sizeof(struct timeval));
    memcpy(rt->timeout, timeout, sizeof(struct timeval));
    rt->cb = cb;
    rt->arg = arg;
    timeout_set(ev, rtimeout_cb, rt);
    timeout_add(ev, timeout);
  } else {
    timeout_set(ev, cb, arg);
    timeout_add(ev, timeout);
  }
  // XXX check calls to timeout_set / timeout_add for failure
  return rt;
}

void libevent_timeout_del(void *timeout)
{
  struct rtimeout *rt = (struct rtimeout *)timeout;

  event_del(rt->ev);
  free(rt->ev);

  if(rt->timeout) {
    free(rt->timeout);
  }

  free(rt);
}

vde_event_handler libevent_eh = {
  .event_add = libevent_event_add,
  .event_del = libevent_event_del,
  .timeout_add = libevent_timeout_add,
  .timeout_del = libevent_timeout_del,
};
