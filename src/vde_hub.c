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
#include <stdlib.h>

#if defined _VDE_EVQUICK
#  include "libevquick/libevquick.h"
#  include <signal.h>
#  define event_init() evquick_init()
#  define event_dispatch() evquick_loop()
extern vde_event_handler evquick_eh;
#define get_event_eh() (&evquick_eh)
#elif defined  _VDE_LIBEVENT
#  include <event.h>
#  include <signal.h>
extern vde_event_handler libevent_eh;
#  define get_event_eh() (&libevent_eh)
#else
#	 define EV_MULTIPLICITY 0
#	 define EV_FORK_ENABLE 0
#	 define EV_STANDALONE 1
#	 include <libev/ev.h>
#	 define event_init()  if(!ev_default_loop(0)) {	\
    perror("ev_default_loop");	\
    exit(1);	\
   }
#  define event_dispatch()   ev_run(0)
extern vde_event_handler libev_eh;
#define get_event_eh() (&libev_eh)
#endif

void sighandler(int signo)
{
  printf("Exiting...\n");
  exit(0);
}


int main(int argc, char **argv)
{
  int res;
  vde_context *ctx;
  vde_component *transport, *engine, *cm;
  vde_component *ctransport, *cengine, *ccm;
  vde_sobj *params;

  event_init();

  res = vde_context_new(&ctx);
  if (res) {
    printf("no new ctx, %d\n", res);
  }

  res = vde_context_init(ctx, get_event_eh(), NULL);
  if (res) {
    printf("no init ctx: %d\n", res);
  }

  params = vde_sobj_from_string("{'path': '/tmp/vde3_test'}");
  res = vde_context_new_component(ctx, VDE_TRANSPORT, "vde2", "tr1", &transport,
                                  params);
  if (res) {
    printf("no new transport: %d\n", res);
  }
  vde_sobj_put(params);

  res = vde_context_new_component(ctx, VDE_ENGINE, "hub", "e1", &engine, NULL);
  if (res) {
    printf("no new engine: %d\n", res);
  }

  params = vde_sobj_from_string("{'engine': 'e1', 'transport': 'tr1'}");
  res = vde_context_new_component(ctx, VDE_CONNECTION_MANAGER, "default", "cm1",
                                  &cm, params);
  if (res) {
    printf("no new cm: %d\n", res);
  }
  vde_sobj_put(params);

  res = vde_conn_manager_listen(cm);
  if (res) {
    printf("no listen on cm: %d\n", res);
  }

  // control part
  params = vde_sobj_from_string("{'path': '/tmp/vde3_test_ctrl'}");
  res = vde_context_new_component(ctx, VDE_TRANSPORT, "vde2", "tr2", &ctransport,
                                  params);
  if (res) {
    printf("no new ctransport: %d\n", res);
  }
  vde_sobj_put(params);

  res = vde_context_new_component(ctx, VDE_ENGINE, "ctrl", "e2", &cengine, NULL);
  if (res) {
    printf("no new cengine: %d\n", res);
  }

  params = vde_sobj_from_string("{'engine': 'e2', 'transport': 'tr2'}");
  res = vde_context_new_component(ctx, VDE_CONNECTION_MANAGER, "default", "cm2",
                                  &ccm, params);
  if (res) {
    printf("no new ccm: %d\n", res);
  }
  vde_sobj_put(params);

  res = vde_conn_manager_listen(ccm);
  if (res) {
    printf("no listen on ccm: %d\n", res);
  }

  signal(SIGINT, &sighandler);
  event_dispatch();

  return 0;
}
