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


/* Standard way for embedding this library */
#define EV_MULTIPLICITY 0
#define EV_NO_THREADS 1
#define EV_USE_4HEAP 1
#define EV_USE_POLL 0
#define EV_USE_EPOLL 1
#define EV_USE_SELECT 0
#define EV_USE_NANOSLEEP 1
#define EV_USE_REALTIME 0
#define EV_USE_FLOOR 1
#define EV_IDLE_ENABLE 0
#define EV_PERIODIC_ENABLE 0
#define EV_STANDALONE 1
#include <libev/ev.c>


#define vde_to_libev(e) ((int) (((VDE_EV_READ & e)?EV_READ:0) | (((VDE_EV_WRITE & e)?EV_WRITE:0))))
#define libev_to_vde(r) ((short)(r & 0xFFFF))
#define TIMEVAL_TO_SEC(x) ((double)(x->tv_sec) + ((double)(x->tv_usec) * (.000001)))


struct libev_event {
  ev_io io;
  short events;
  short revents;
  vde_event_cb cb;
  int fd;
  void *arg;
};

struct libev_timer {
  ev_timer timer;
  vde_event_cb cb;
  void *arg;
};

typedef struct libev_event libev_event;
typedef struct libev_timer libev_timer;

/*
 * vde_event_handler which uses libev as a backend, handling of recurrent
 * timeouts (aka periodic) is implemented as well.
 */


void event_callback(ev_io *w_, int revents)
{
  libev_event *ev = (libev_event *) w_;
  ev->revents = libev_to_vde(revents);
  ev->cb(ev->fd, ev->revents, ev->arg);
}

void timeout_callback(ev_timer *w_, int revents)
{
  libev_timer *t = (libev_timer *) w_;
  short rf = libev_to_vde(revents);
  t->cb(-1, rf, t->arg);
}

void *libev_event_add(int fd, short events, const struct timeval *timeout,
                         vde_event_cb cb, void *arg)
{
  libev_event *ev;
  int ev_flags = 0;
  ev = (libev_event *)malloc(sizeof(libev_event));
  if (!ev) {
    vde_error("%s: can't allocate memory for new event", __PRETTY_FUNCTION__);
    errno = ENOMEM;
    return NULL;
  }
  ev_flags = vde_to_libev(events);
  ev->fd = fd;
  ev->cb = cb;
  ev->arg = arg;
  ev_io_init(&ev->io, event_callback, fd, ev_flags);
  ev_io_start(&ev->io);
  return ev;
}

void libev_event_del(void *event)
{
  struct libev_event *ev = (struct libev_event *)event;
  ev_io_stop(&ev->io);
  free(ev);
}

void *libev_timeout_add(const struct timeval *timeout, short events,
                           vde_event_cb cb, void *arg)
{
  libev_timer *t;
  double one_time, retrig;
  t = (libev_timer *)malloc(sizeof(libev_timer));
  if (!t) {
    vde_error("%s: can't allocate memory for timeout", __PRETTY_FUNCTION__);
    errno = ENOMEM;
    return NULL;
  }
  one_time = TIMEVAL_TO_SEC(timeout);
  if (events & VDE_EV_PERSIST)
    retrig = one_time;
  else
    retrig = 0.;
  t->cb = cb;
  t->arg = arg;
  ev_timer_init(&t->timer, timeout_callback, one_time, retrig);
  ev_timer_start(&t->timer);
  return t;
}

void libev_timeout_del(void *timeout)
{
  libev_timer *t = (libev_timer *) timeout;
  ev_timer_stop(&t->timer);
  free(t);
}



vde_event_handler libev_eh = {
  .event_add = libev_event_add,
  .event_del = libev_event_del,
  .timeout_add = libev_timeout_add,
  .timeout_del = libev_timeout_del,
};
