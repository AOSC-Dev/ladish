/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2009, 2010, 2011 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains interface to graph dispatcher object
 **************************************************************************
 *
 * LADI Session Handler is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * LADI Session Handler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LADI Session Handler. If not, see <http://www.gnu.org/licenses/>
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef JACK_DISPATCH_H__C7566B66_081D_4D00_A702_7C18F7CC0735__INCLUDED
#define JACK_DISPATCH_H__C7566B66_081D_4D00_A702_7C18F7CC0735__INCLUDED

#include "common.h"
#include "../proxies/graph_proxy.h"
#include "graph.h"
#include "app_supervisor.h"

typedef struct ladish_virtualizer { int unused; } * ladish_virtualizer_handle;

bool
ladish_virtualizer_create(
  graph_proxy_handle jack_graph_proxy,
  ladish_graph_handle jack_graph,
  ladish_virtualizer_handle * handle_ptr);

void
ladish_virtualizer_set_graph_connection_handlers(
  ladish_virtualizer_handle handle,
  ladish_graph_handle graph);

unsigned int
ladish_virtualizer_get_our_clients_count(
  ladish_virtualizer_handle handle);

bool
ladish_virtualizer_is_hidden_app(
  ladish_graph_handle jack_graph,
  const uuid_t app_uuid,
  const char * app_name);

void
ladish_virtualizer_remove_app(
  ladish_graph_handle jack_graph,
  const uuid_t app_uuid,
  const char * app_name);

void
ladish_virtualizer_rename_app(
  void * vgraph_context,
  const uuid_t uuid,
  const char * old_name,
  const char * new_app_name);

bool
ladish_virtualizer_is_system_client(
  uuid_t uuid);

bool
ladish_virtualizer_is_a2j_client(
  ladish_client_handle jclient);

void
ladish_virtualizer_destroy(
  ladish_virtualizer_handle handle);

/* create new client and move capture ports of the supplied client the the new client */
bool
ladish_virtualizer_split_client(
  ladish_graph_handle vgraph,
  uint64_t client_id);

/* move ports of client2 to client1 and remove client2 */
bool
ladish_virtualizer_join_clients(
  ladish_graph_handle vgraph,
  uint64_t client1_id,
  uint64_t client2_id);

ladish_app_handle
ladish_find_app_by_pid(
  pid_t pid,
  ladish_graph_handle * graph_ptr);

#endif /* #ifndef JACK_DISPATCH_H__C7566B66_081D_4D00_A702_7C18F7CC0735__INCLUDED */
