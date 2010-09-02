/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2010 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains the parts of room object implementation
 * that are related to project load functionality
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <expat.h>

#include "room_internal.h"
#include "../catdup.h"
#include "load.h"
#include "../proxies/notify_proxy.h"
#include "escape.h"
#include "studio.h"

#define context_ptr ((struct ladish_parse_context *)data)
#define room_ptr ((struct ladish_room *)context_ptr->room)

static void callback_chrdata(void * data, const XML_Char * s, int len)
{
  if (context_ptr->error)
  {
    return;
  }

  if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_PARAMETER ||
      context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_KEY ||
      context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_APPLICATION)
  {
    if (context_ptr->data_used + len >= sizeof(context_ptr->data))
    {
      log_error("xml parse max char data length reached");
      context_ptr->error = XML_TRUE;
      return;
    }

    memcpy(context_ptr->data + context_ptr->data_used, s, len);
    context_ptr->data_used += len;
  }
}

static void callback_elstart(void * data, const char * el, const char ** attr)
{
  const char * name;
  const char * uuid_str;
  uuid_t uuid;
  const char * uuid2_str;
  uuid_t uuid2;
  ladish_port_handle port1;
  ladish_port_handle port2;
  uint32_t port_type;
  uint32_t port_flags;

  if (context_ptr->error)
  {
    return;
  }

  if (context_ptr->depth + 1 >= MAX_STACK_DEPTH)
  {
    log_error("xml parse max stack depth reached");
    context_ptr->error = XML_TRUE;
    return;
  }

  if (strcmp(el, "project") == 0)
  {
    //log_info("<project>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_PROJECT;

    if (!ladish_get_name_and_uuid_attributes("/project", attr, &name, &uuid_str, uuid))
    {
      context_ptr->error = XML_TRUE;
      return;
    }

    log_info("Project '%s' with uuid %s", name, uuid_str);

    room_ptr->project_name = strdup(name);
    if (room_ptr->project_name == NULL)
    {
      log_error("strdup() failed for project name");
      context_ptr->error = XML_TRUE;
      return;
    }

    uuid_copy(room_ptr->project_uuid, uuid);

    return;
  }

  if (strcmp(el, "jack") == 0)
  {
    //log_info("<jack>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_JACK;
    return;
  }

  if (strcmp(el, "clients") == 0)
  {
    //log_info("<clients>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_CLIENTS;
    return;
  }

  if (strcmp(el, "room") == 0)
  {
    //log_info("<room>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_ROOM;
    return;
  }

  if (strcmp(el, "client") == 0)
  {
    //log_info("<client>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_CLIENT;
    if (context_ptr->client != NULL)
    {
        log_error("nested clients");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (context_ptr->depth == 3 &&
        context_ptr->element[0] == PARSE_CONTEXT_PROJECT &&
        context_ptr->element[1] == PARSE_CONTEXT_JACK &&
        context_ptr->element[2] == PARSE_CONTEXT_CLIENTS)
    {
      if (!ladish_get_name_and_uuid_attributes("/project/jack/clients/client", attr, &name, &uuid_str, uuid))
      {
        context_ptr->error = XML_TRUE;
        return;
      }

      log_info("jack client \"%s\" with uuid %s", name, uuid_str);

      if (!ladish_client_create(uuid, &context_ptr->client))
      {
        log_error("ladish_client_create() failed.");
        context_ptr->error = XML_TRUE;
        ASSERT(context_ptr->client == NULL);
        return;
      }

      if (!ladish_graph_add_client(ladish_studio_get_jack_graph(), context_ptr->client, name, true))
      {
        log_error("ladish_graph_add_client() failed to add client '%s' to JACK graph", name);
        context_ptr->error = XML_TRUE;
        ladish_client_destroy(context_ptr->client);
        context_ptr->client = NULL;
      }

      return;
    }

    if (context_ptr->depth == 2 &&
        context_ptr->element[0] == PARSE_CONTEXT_PROJECT &&
        context_ptr->element[1] == PARSE_CONTEXT_CLIENTS)
    {
      if (!ladish_get_name_and_uuid_attributes("/studio/clients/client", attr, &name, &uuid_str, uuid))
      {
        context_ptr->error = XML_TRUE;
        return;
      }

      log_info("studio client \"%s\" with uuid %s", name, uuid_str);

      context_ptr->client = ladish_graph_find_client_by_uuid(room_ptr->graph, uuid);
      if (context_ptr->client != NULL)
      {
        log_info("Found existing client");
        return;
      }

      if (!ladish_client_create(uuid, &context_ptr->client))
      {
        log_error("ladish_client_create() failed.");
        context_ptr->error = XML_TRUE;
        ASSERT(context_ptr->client == NULL);
        return;
      }

      if (!ladish_graph_add_client(room_ptr->graph, context_ptr->client, name, true))
      {
        log_error("ladish_graph_add_client() failed to add client '%s' to room graph", name);
        context_ptr->error = XML_TRUE;
        ladish_client_destroy(context_ptr->client);
        context_ptr->client = NULL;
      }

      return;
    }

    log_error("client element in wrong place");
    ladish_dump_element_stack(context_ptr);
    context_ptr->error = XML_TRUE;
    return;
  }

  if (strcmp(el, "ports") == 0)
  {
    //log_info("<ports>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_PORTS;
    return;
  }

  if (strcmp(el, "port") == 0)
  {
    //log_info("<port>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_PORT;

    if (context_ptr->port != NULL)
    {
        log_error("nested ports");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (context_ptr->depth >= 3 &&
        context_ptr->element[context_ptr->depth - 3] == PARSE_CONTEXT_CLIENTS &&
        context_ptr->element[context_ptr->depth - 2] == PARSE_CONTEXT_CLIENT &&
        context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_PORTS)
    {
      //log_info("client port");
      if (context_ptr->client == NULL)
      {
        log_error("client-less port");
        context_ptr->error = XML_TRUE;
        return;
      }

      if (context_ptr->depth == 5 && context_ptr->element[0] == PARSE_CONTEXT_PROJECT && context_ptr->element[1] == PARSE_CONTEXT_JACK)
      {
        if (!ladish_get_name_and_uuid_attributes("/project/jack/clients/client/ports/port", attr, &name, &uuid_str, uuid))
        {
          context_ptr->error = XML_TRUE;
          return;
        }

        log_info("jack port \"%s\" with uuid %s", name, uuid_str);

        if (!ladish_port_create(uuid, false, &context_ptr->port))
        {
          log_error("ladish_port_create() failed.");
          return;
        }

        if (!ladish_graph_add_port(ladish_studio_get_jack_graph(), context_ptr->client, context_ptr->port, name, 0, 0, true))
        {
          log_error("ladish_graph_add_port() failed.");
          ladish_port_destroy(context_ptr->port);
          context_ptr->port = NULL;
        }

        return;
      }
      else if (context_ptr->depth == 4 && context_ptr->element[0] == PARSE_CONTEXT_PROJECT)
      {
        if (!ladish_get_name_and_uuid_attributes("/project/clients/client/ports/port", attr, &name, &uuid_str, uuid))
        {
          context_ptr->error = XML_TRUE;
          return;
        }

        context_ptr->port = ladish_graph_find_port_by_uuid(room_ptr->graph, uuid, false);
        if (context_ptr->port != NULL)
        {
          if (!ladish_port_is_link(context_ptr->port))
          {
            log_info("port \"%s\" with uuid %s already exists in room graph and is not a room-studio link", name, uuid_str);
            context_ptr->error = XML_TRUE;
          }
          return;
        }

        context_ptr->port = ladish_graph_find_port_by_uuid(ladish_studio_get_jack_graph(), uuid, false);
        if (context_ptr->port == NULL)
        {
          log_info("app port \"%s\" with uuid %s not found in the jack graph", name, uuid_str);
          context_ptr->error = XML_TRUE;
          ladish_graph_dump(ladish_studio_get_jack_graph());
          return;
        }

        log_info("app port \"%s\" with uuid %s", name, uuid_str);

        if (!ladish_graph_add_port(room_ptr->graph, context_ptr->client, context_ptr->port, name, 0, 0, true))
        {
          log_error("ladish_graph_add_port() failed.");
          ladish_port_destroy(context_ptr->port);
          context_ptr->port = NULL;
          return;
        }

        return;
      }
    }
    else if (context_ptr->depth == 2 &&
             context_ptr->element[0] == PARSE_CONTEXT_PROJECT &&
             context_ptr->element[1] == PARSE_CONTEXT_ROOM)
    {
      ASSERT(context_ptr->room != NULL);
      //log_info("room port");

      if (!ladish_get_name_and_uuid_attributes("/project/room/port", attr, &name, &uuid_str, uuid))
      {
        context_ptr->error = XML_TRUE;
        return;
      }

      log_info("room port \"%s\" with uuid %s", name, uuid_str);

      if (!ladish_parse_port_type_and_direction_attributes("/project/room/port", attr, &port_type, &port_flags))
      {
        context_ptr->error = XML_TRUE;
        return;
      }

      context_ptr->port = ladish_graph_find_port_by_uuid(room_ptr->graph, uuid, false);
      if (context_ptr->port == NULL)
      {
        log_error("Cannot find room link port() failed.");
        context_ptr->port = NULL;
      }

      return;
    }

    log_error("port element in wrong place");
    ladish_dump_element_stack(context_ptr);
    context_ptr->error = XML_TRUE;

    return;
  }

  if (strcmp(el, "connections") == 0)
  {
    //log_info("<connections>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_CONNECTIONS;
    return;
  }

  if (strcmp(el, "connection") == 0)
  {
    //log_info("<connection>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_CONNECTION;

    uuid_str = ladish_get_uuid_attribute(attr, "port1", uuid, false);
    if (uuid_str == NULL)
    {
      log_error("/studio/connections/connection \"port1\" attribute is not available.");
      context_ptr->error = XML_TRUE;
      return;
    }

    uuid2_str = ladish_get_uuid_attribute(attr, "port2", uuid2, false);
    if (uuid2_str == NULL)
    {
      log_error("/studio/connections/connection \"port2\" attribute is not available.");
      context_ptr->error = XML_TRUE;
      return;
    }

    log_info("studio connection between port %s and port %s", uuid_str, uuid2_str);

    port1 = ladish_graph_find_port_by_uuid(room_ptr->graph, uuid, true);
    if (port1 == NULL)
    {
      log_error("studio client with unknown port %s", uuid_str);
      context_ptr->error = XML_TRUE;
      return;
    }

    port2 = ladish_graph_find_port_by_uuid(room_ptr->graph, uuid2, true);
    if (port2 == NULL)
    {
      log_error("studio client with unknown port %s", uuid2_str);
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->connection_id = ladish_graph_add_connection(room_ptr->graph, port1, port2, true);
    if (context_ptr->connection_id == 0)
    {
      log_error("ladish_graph_add_connection() failed.");
      return;
    }

    return;
  }

  if (strcmp(el, "applications") == 0)
  {
    //log_info("<applications>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_APPLICATIONS;
    return;
  }

  if (strcmp(el, "application") == 0)
  {
    //log_info("<application>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_APPLICATION;

    name = ladish_get_string_attribute(attr, "name");
    if (name == NULL)
    {
      log_error("application \"name\" attribute is not available.");
      context_ptr->error = XML_TRUE;
      return;
    }

    if (ladish_get_bool_attribute(attr, "terminal", &context_ptr->terminal) == NULL)
    {
      log_error("application \"terminal\" attribute is not available. name=\"%s\"", name);
      context_ptr->error = XML_TRUE;
      return;
    }

    if (ladish_get_bool_attribute(attr, "autorun", &context_ptr->autorun) == NULL)
    {
      log_error("application \"autorun\" attribute is not available. name=\"%s\"", name);
      context_ptr->error = XML_TRUE;
      return;
    }

    if (ladish_get_byte_attribute(attr, "level", &context_ptr->level) == NULL)
    {
      log_error("application \"level\" attribute is not available. name=\"%s\"", name);
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->str = strdup(name);
    if (context_ptr->str == NULL)
    {
      log_error("strdup() failed");
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->data_used = 0;
    return;
  }

  if (strcmp(el, "dict") == 0)
  {
    //log_info("<dict>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_DICT;

    if (context_ptr->dict != NULL)
    {
        log_error("nested dicts");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (context_ptr->depth == 1 &&
        context_ptr->element[0] == PARSE_CONTEXT_STUDIO)
    {
      context_ptr->dict = ladish_graph_get_dict(room_ptr->graph);
      ASSERT(context_ptr->dict != NULL);
    }
    else if (context_ptr->depth > 0 &&
             context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_CLIENT)
    {
      ASSERT(context_ptr->client != NULL);
      context_ptr->dict = ladish_client_get_dict(context_ptr->client);
      ASSERT(context_ptr->dict != NULL);
    }
    else if (context_ptr->depth > 0 &&
             context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_PORT)
    {
      ASSERT(context_ptr->port != NULL);
      context_ptr->dict = ladish_port_get_dict(context_ptr->port);
      ASSERT(context_ptr->dict != NULL);
    }
    else if (context_ptr->depth > 0 &&
             context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_CONNECTION)
    {
      ASSERT(context_ptr->port != NULL);
      context_ptr->dict = ladish_graph_get_connection_dict(room_ptr->graph, context_ptr->connection_id);
      ASSERT(context_ptr->dict != NULL);
    }
    else
    {
      log_error("unexpected dict XML element");
      context_ptr->error = XML_TRUE;
      return;
    }

    return;
  }

  if (strcmp(el, "key") == 0)
  {
    //log_info("<key>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_KEY;

    if (context_ptr->dict == NULL)
    {
        log_error("dict-less key");
        context_ptr->error = XML_TRUE;
        return;
    }

    name = ladish_get_string_attribute(attr, "name");
    if (name == NULL)
    {
      log_error("dict/key \"name\" attribute is not available.");
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->str = strdup(name);
    if (context_ptr->str == NULL)
    {
      log_error("strdup() failed");
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->data_used = 0;

    return;
  }

  log_error("unknown element \"%s\"", el);
  context_ptr->error = XML_TRUE;
}

static void callback_elend(void * data, const char * el)
{
  if (context_ptr->error)
  {
    return;
  }

  //log_info("element end (depth = %d, element = %u)", context_ptr->depth, context_ptr->element[context_ptr->depth]);

  if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_KEY &&
      context_ptr->depth > 0 &&
      context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_DICT)
  {
    ASSERT(context_ptr->dict != NULL);
    context_ptr->data[context_ptr->data_used] = 0;
    log_info("dict key '%s' with value '%s'", context_ptr->str, context_ptr->data);
    if (!ladish_dict_set(context_ptr->dict, context_ptr->str, context_ptr->data))
    {
      log_error("ladish_dict_set() failed");
      context_ptr->error = XML_TRUE;
      return;
    }
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_ROOM)
  {
    //log_info("</room>");
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_JACK)
  {
    //log_info("</jack>");
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_DICT)
  {
    //log_info("</dict>");
    ASSERT(context_ptr->dict != NULL);
    context_ptr->dict = NULL;
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_CLIENT)
  {
    //log_info("</client>");
    ASSERT(context_ptr->client != NULL);
    context_ptr->client = NULL;
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_PORT)
  {
    //log_info("</port>");
    ASSERT(context_ptr->port != NULL);
    context_ptr->port = NULL;
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_APPLICATION)
  {
    context_ptr->data[unescape(context_ptr->data, context_ptr->data_used, context_ptr->data)] = 0;
    unescape(context_ptr->str, strlen(context_ptr->str) + 1, context_ptr->str);

    log_info("application '%s' (%s, %s, level %u) with commandline '%s'", context_ptr->str, context_ptr->terminal ? "terminal" : "shell", context_ptr->autorun ? "autorun" : "stopped", (unsigned int)context_ptr->level, context_ptr->data);

    if (ladish_app_supervisor_add(room_ptr->app_supervisor, context_ptr->str, context_ptr->autorun, context_ptr->data, context_ptr->terminal, context_ptr->level) == NULL)
    {
      log_error("ladish_app_supervisor_add() failed.");
      context_ptr->error = XML_TRUE;
    }
  }

  context_ptr->depth--;

  if (context_ptr->str != NULL)
  {
    free(context_ptr->str);
    context_ptr->str = NULL;
  }

  return;
}

#undef room_ptr
#undef context_ptr

#define room_ptr ((struct ladish_room *)room_handle)

bool ladish_room_load_project(ladish_room_handle room_handle, const char * project_dir)
{
  char * path;
  struct stat st;
  XML_Parser parser;
  int bytes_read;
  void * buffer;
  int fd;
  enum XML_Status xmls;
  struct ladish_parse_context parse_context;
  bool ret;

  log_info("Loading project '%s' into room '%s'", project_dir, room_ptr->name);

  ASSERT(room_ptr->project_dir == NULL);
  ASSERT(room_ptr->project_name == NULL);
  ASSERT(!ladish_app_supervisor_has_apps(room_ptr->app_supervisor));

  ret = false;

  room_ptr->project_dir = strdup(project_dir);
  if (room_ptr->project_dir == NULL)
  {
    log_error("strdup() failed to for project dir");
    goto exit;
  }

  path = catdup(project_dir, LADISH_PROJECT_FILENAME);
  if (path == NULL)
  {
    log_error("catdup() failed to compose xml file path");
    goto exit;
  }

  if (stat(path, &st) != 0)
  {
    log_error("failed to stat '%s': %d (%s)", path, errno, strerror(errno));
    goto free_path;
  }

  fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    log_error("failed to open '%s': %d (%s)", path, errno, strerror(errno));
    goto free_path;
  }

  parser = XML_ParserCreate(NULL);
  if (parser == NULL)
  {
    log_error("XML_ParserCreate() failed to create parser object.");
    goto close;
  }

  //log_info("conf file size is %llu bytes", (unsigned long long)st.st_size);

  /* we are expecting that conf file has small enough size to fit in memory */

  buffer = XML_GetBuffer(parser, st.st_size);
  if (buffer == NULL)
  {
    log_error("XML_GetBuffer() failed.");
    goto free_parser;
  }

  bytes_read = read(fd, buffer, st.st_size);
  if (bytes_read != st.st_size)
  {
    log_error("read() returned unexpected result.");
    goto free_parser;
  }

  //log_info("\n----------\n%s\n-----------\n", buffer);
  //goto free_parser;

  parse_context.error = XML_FALSE;
  parse_context.depth = -1;
  parse_context.str = NULL;
  parse_context.client = NULL;
  parse_context.port = NULL;
  parse_context.dict = NULL;
  parse_context.room = room_handle;

  XML_SetElementHandler(parser, callback_elstart, callback_elend);
  XML_SetCharacterDataHandler(parser, callback_chrdata);
  XML_SetUserData(parser, &parse_context);

  xmls = XML_ParseBuffer(parser, bytes_read, XML_TRUE);
  if (xmls == XML_STATUS_ERROR && !parse_context.error)
  {
    log_error("XML_ParseBuffer() failed.");
  }
  if (xmls == XML_STATUS_ERROR || parse_context.error)
  {
    goto free_parser;
  }

  ladish_interlink_clients(room_ptr->graph, room_ptr->app_supervisor);
  /* ladish_graph_dump(ladish_studio_get_jack_graph()); */
  /* ladish_graph_dump(room_ptr->graph); */

  ladish_app_supervisor_set_directory(room_ptr->app_supervisor, project_dir);
  ladish_graph_trick_dicts(room_ptr->graph);
  ladish_app_supervisor_autorun(room_ptr->app_supervisor);

  ret = true;

free_parser:
  XML_ParserFree(parser);
close:
  close(fd);
free_path:
  free(path);
exit:
  if (!ret)
  {
    ladish_room_unload_project(room_handle);
    ladish_notify_simple(LADISH_NOTIFY_URGENCY_HIGH, "Project load failed", "Please inspect the ladishd log (~/.ladish/ladish.log) for more info");
  }

  return ret;
}

#undef room_ptr