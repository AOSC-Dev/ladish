/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2009, 2010, 2011 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains the implementation of the client objects
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

#include "common.h"
#include "client.h"

struct ladish_client
{
  uuid_t uuid;                             /* The UUID of the client */
  uuid_t uuid_interlink;                   /* The UUID of the linked client (vgraph <-> jack graph) */
  uuid_t uuid_app;                         /* The UUID of the app that owns this client */
  uint64_t jack_id;                        /* JACK client ID */
  char * jack_name;                        /* JACK client name */
  pid_t pid;                               /* process id. */
  bool has_js_callback;                    /* Whether the client has set jack session callback */
  ladish_dict_handle dict;
  void * vgraph;                /* virtual graph */
};

bool
ladish_client_create(
  const uuid_t uuid_ptr,
  ladish_client_handle * client_handle_ptr)
{
  struct ladish_client * client_ptr;

  client_ptr = malloc(sizeof(struct ladish_client));
  if (client_ptr == NULL)
  {
    log_error("malloc() failed to allocate struct ladish_client");
    return false;
  }

  if (!ladish_dict_create(&client_ptr->dict))
  {
    log_error("ladish_dict_create() failed for client");
    free(client_ptr);
    return false;
  }

  if (uuid_ptr == NULL)
  {
    uuid_generate(client_ptr->uuid);
  }
  else
  {
    uuid_copy(client_ptr->uuid, uuid_ptr);
  }

  uuid_clear(client_ptr->uuid_interlink);
  uuid_clear(client_ptr->uuid_app);

  client_ptr->jack_id = 0;
  client_ptr->jack_name = NULL;
  client_ptr->pid = 0;
  client_ptr->has_js_callback = false;
  client_ptr->vgraph = NULL;

#if 0
  {
    char str[37];
    uuid_unparse(client_ptr->uuid, str);
    log_info("Created client %s", str);
  }
#endif

  log_info("client %p created", client_ptr);
  *client_handle_ptr = (ladish_client_handle)client_ptr;
  return true;
}

#define client_ptr ((struct ladish_client *)client_handle)

bool
ladish_client_create_copy(
  ladish_client_handle client_handle,
  ladish_client_handle * client_handle_ptr)
{
  return ladish_client_create(client_ptr->uuid, client_handle_ptr);
}

void
ladish_client_destroy(
  ladish_client_handle client_handle)
{
  log_info("client %p destroy", client_ptr);

  ladish_dict_destroy(client_ptr->dict);
  free(client_ptr->jack_name);
  free(client_ptr);
}

ladish_dict_handle ladish_client_get_dict(ladish_client_handle client_handle)
{
  return client_ptr->dict;
}

void ladish_client_get_uuid(ladish_client_handle client_handle, uuid_t uuid)
{
  uuid_copy(uuid, client_ptr->uuid);
}

void ladish_client_set_jack_id(ladish_client_handle client_handle, uint64_t jack_id)
{
  log_info("client jack id set to %"PRIu64, jack_id);
  client_ptr->jack_id = jack_id;
}

uint64_t ladish_client_get_jack_id(ladish_client_handle client_handle)
{
  return client_ptr->jack_id;
}

void ladish_client_set_jack_name(ladish_client_handle client_handle, const char * jack_name)
{
  char * name_dup;

  name_dup = strdup(jack_name);
  if (name_dup == NULL)
  {
    log_error("stdup(\"%s\") failed", jack_name);
    return;
  }

  free(client_ptr->jack_name);
  client_ptr->jack_name = name_dup;
}

const char * ladish_client_get_jack_name(ladish_client_handle client_handle)
{
  return client_ptr->jack_name;
}

void ladish_client_set_pid(ladish_client_handle client_handle, pid_t pid)
{
  client_ptr->pid = pid;
}

pid_t ladish_client_get_pid(ladish_client_handle client_handle)
{
  return client_ptr->pid;
}

void ladish_client_set_vgraph(ladish_client_handle client_handle, void * vgraph)
{
  client_ptr->vgraph = vgraph;
}

void * ladish_client_get_vgraph(ladish_client_handle client_handle)
{
  return client_ptr->vgraph;
}

#define client2_ptr ((struct ladish_client *)client2_handle)

void ladish_client_interlink(ladish_client_handle client_handle, ladish_client_handle client2_handle)
{
  uuid_copy(client_ptr->uuid_interlink, client2_ptr->uuid);
  uuid_copy(client2_ptr->uuid_interlink, client_ptr->uuid);
}

void ladish_client_interlink_copy(ladish_client_handle client_handle, ladish_client_handle client2_handle)
{
  uuid_copy(client_ptr->uuid_interlink, client2_ptr->uuid_interlink);
}

void ladish_client_copy_app(ladish_client_handle client_handle, ladish_client_handle client2_handle)
{
  uuid_copy(client_ptr->uuid_app, client2_ptr->uuid_app);
}

#undef client2_ptr

bool ladish_client_get_interlink(ladish_client_handle client_handle, uuid_t uuid)
{
  if (uuid_is_null(client_ptr->uuid_interlink))
  {
    return false;
  }

  uuid_copy(uuid, client_ptr->uuid_interlink);
  return true;
}

bool ladish_client_set_interlink(ladish_client_handle client_handle, uuid_t uuid)
{
  if (uuid_is_null(client_ptr->uuid_interlink))
  {
    return false;
  }

  uuid_copy(uuid, client_ptr->uuid_interlink);
  return true;
}

void ladish_client_clear_interlink(ladish_client_handle client_handle)
{
  uuid_clear(client_ptr->uuid_interlink);
}

void ladish_client_set_app(ladish_client_handle client_handle, const uuid_t uuid)
{
  uuid_copy(client_ptr->uuid_app, uuid);
}

bool ladish_client_get_app(ladish_client_handle client_handle, uuid_t uuid)
{
  if (uuid_is_null(client_ptr->uuid_app))
  {
    return false;
  }

  uuid_copy(uuid, client_ptr->uuid_app);
  return true;
}

bool ladish_client_is_app(ladish_client_handle client_handle, uuid_t uuid)
{
  return uuid_compare(uuid, client_ptr->uuid_app) == 0;
}

bool ladish_client_has_app(ladish_client_handle client_handle)
{
  return !uuid_is_null(client_ptr->uuid_app);
}

void ladish_client_set_js(ladish_client_handle client_handle, bool has_js_callback)
{
  client_ptr->has_js_callback = has_js_callback;
}

bool ladish_client_is_js(ladish_client_handle client_handle)
{
  return client_ptr->has_js_callback;
}

#undef client_ptr
