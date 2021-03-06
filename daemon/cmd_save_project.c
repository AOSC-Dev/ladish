/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2010,2011 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains implementation of the "save project" command
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

#include "cmd.h"
#include "room.h"
#include "studio.h"
#include "../proxies/notify_proxy.h"

struct ladish_command_save_project
{
  struct ladish_command command;
  uuid_t room_uuid;
  char * project_dir;
  char * project_name;
  bool done;
  bool success;
};

#define cmd_ptr ((struct ladish_command_save_project *)command_context)

void ladish_room_project_save_complete(void * command_context, bool success)
{
  cmd_ptr->done = true;
  cmd_ptr->success = success;

  if (!success)
  {
    ladish_notify_simple(LADISH_NOTIFY_URGENCY_HIGH, "Project save failed", LADISH_CHECK_LOG_TEXT);
  }
}

static bool run(void * command_context)
{
  ladish_room_handle room;

  if (cmd_ptr->command.state == LADISH_COMMAND_STATE_WAITING)
  {
    if (!cmd_ptr->done)
    {
      return true;
    }

    cmd_ptr->command.state = LADISH_COMMAND_STATE_DONE;
    return cmd_ptr->success;
  }

  if (cmd_ptr->command.state != LADISH_COMMAND_STATE_PENDING)
  {
    ASSERT_NO_PASS;
    return false;
  }

  room = ladish_studio_find_room_by_uuid(cmd_ptr->room_uuid);
  if (room == NULL)
  {
    log_error("Cannot save project of unknown room");
    ladish_notify_simple(LADISH_NOTIFY_URGENCY_HIGH, "Cannot save project of unknown room", NULL);
    return false;
  }

  cmd_ptr->command.state = LADISH_COMMAND_STATE_WAITING;

  ladish_room_save_project(room, cmd_ptr->project_dir, cmd_ptr->project_name, cmd_ptr, ladish_room_project_save_complete);

  return true;
}

static void destructor(void * command_context)
{
  log_info("save project command destructor");
  free(cmd_ptr->project_name);
  free(cmd_ptr->project_dir);
}

#undef cmd_ptr

bool
ladish_command_save_project(
  void * call_ptr,
  struct ladish_cqueue * queue_ptr,
  const uuid_t room_uuid_ptr,
  const char * project_dir,
  const char * project_name)
{
  struct ladish_command_save_project * cmd_ptr;
  char * project_dir_dup;
  char * project_name_dup;

  project_dir_dup = strdup(project_dir);
  if (project_dir_dup == NULL)
  {
    cdbus_error(call_ptr, DBUS_ERROR_FAILED, "strdup('%s') failed.", project_dir);
    goto fail;
  }

  project_name_dup = strdup(project_name);
  if (project_name_dup == NULL)
  {
    cdbus_error(call_ptr, DBUS_ERROR_FAILED, "strdup('%s') failed.", project_name);
    goto fail_free_dir;
  }

  cmd_ptr = ladish_command_new(sizeof(struct ladish_command_save_project));
  if (cmd_ptr == NULL)
  {
    log_error("ladish_command_new() failed.");
    goto fail_free_name;
  }

  uuid_copy(cmd_ptr->room_uuid, room_uuid_ptr);

  cmd_ptr->command.run = run;
  cmd_ptr->command.destructor = destructor;
  cmd_ptr->project_dir = project_dir_dup;
  cmd_ptr->project_name = project_name_dup;
  cmd_ptr->done = false;
  cmd_ptr->success = true;

  if (!ladish_cqueue_add_command(queue_ptr, &cmd_ptr->command))
  {
    cdbus_error(call_ptr, DBUS_ERROR_FAILED, "ladish_cqueue_add_command() failed.");
    goto fail_destroy_command;
  }

  return true;

fail_destroy_command:
  free(cmd_ptr);
fail_free_name:
  free(project_name_dup);
fail_free_dir:
  free(project_dir_dup);
fail:
  return false;
}
