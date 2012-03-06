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
#include "libevquick/libevquick.h"

/*
 * vde_event_handler which uses libev as a backend, handling of recurrent
 * timeouts (aka periodic) is implemented as well.
 */

struct timer_wrap {
  evquick_timer *t;
  vde_event_cb cb;
  void *arg;
};

void timeout_callback(void *arg)
{
  struct timer_wrap *tw = (struct timer_wrap *)arg;
  tw->cb(-1, VDE_EV_TIMEOUT, tw->arg);
  if ((tw->t->flags & VDE_EV_PERSIST) == 0)
    free(tw);
}

void *quick_event_add(int fd, short events, const struct timeval *timeout,
                         vde_event_cb cb, void *arg)
{
  short ev = 0;
  if (events & VDE_EV_READ)
    ev |= POLLIN;
  if (events & VDE_EV_WRITE)
    ev |= POLLOUT;
  return evquick_addevent(fd, ev, cb, NULL, arg);
}

void quick_event_del(void *event)
{
  evquick_delevent((evquick_event *)event);
}

void *quick_timeout_add(const struct timeval *timeout, short events,
                           vde_event_cb cb, void *arg)
{

  struct timer_wrap *tw = malloc(sizeof(struct timer_wrap));
  if (!tw)
    return tw;
  tw->cb = cb;
  tw->arg = arg;
  tw->t = evquick_addtimer(timeout->tv_sec * 1000 + timeout->tv_usec / 1000,
                            events&VDE_EV_PERSIST?EVQUICK_EV_RETRIGGER:0,
                            timeout_callback, tw);
  return tw;
}

void quick_timeout_del(void *timeout)
{
  struct timer_wrap *tw = (struct timer_wrap *)timeout;
  evquick_deltimer(tw->t);
  free(tw);
}

vde_event_handler evquick_eh = {
  .event_add = quick_event_add,
  .event_del = quick_event_del,
  .timeout_add = quick_timeout_add,
  .timeout_del = quick_timeout_del,
};
