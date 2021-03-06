/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2009,2010,2011,2012 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains implementation of the graph view object
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
#include "graph_view.h"
#include "gtk_builder.h"
#include "world_tree.h"
#include "menu.h"
#include "../proxies/room_proxy.h"
#include "../common/catdup.h"

struct graph_view
{
  struct list_head siblings;
  char * view_name;
  char * project_name;
  char * full_name;
  graph_canvas_handle graph_canvas;
  graph_proxy_handle graph;
  GtkWidget * canvas_widget;
  ladish_app_supervisor_proxy_handle app_supervisor;
  ladish_room_proxy_handle room;
};

struct list_head g_views;

GtkScrolledWindow * g_main_scrolledwin;
static struct graph_view * g_current_view;
GtkWidget * g_view_label;

const char * g_view_label_text = "";

void view_init(void)
{
  g_view_label_text = _(
  "If you've started ladish for the first time, you should:\n\n"
  " 1. Create a new studio (in the menu, Studio -> New Studio)\n"
  " 2. Configure JACK (in the menu, Tools -> Configure JACK)\n"
  " 3. Start the studio (in the menu, Studio -> Start Studio)\n"
  " 4. Start apps (in the menu, Application -> Run)\n"
  " 5. Connect their ports by click & drag on canvas\n"
  " 6. Save the studio (in the menu, Studio -> Save Studio)\n");

  g_main_scrolledwin = GTK_SCROLLED_WINDOW(get_gtk_builder_widget("main_scrolledwin"));
  INIT_LIST_HEAD(&g_views);

  g_view_label = gtk_label_new(g_view_label_text);
  g_object_ref(g_view_label);
  gtk_widget_show(g_view_label);

  g_current_view = NULL;
  gtk_scrolled_window_add_with_viewport(g_main_scrolledwin, g_view_label);
}

void announce_view_name_change(struct graph_view * view_ptr)
{
  world_tree_name_changed((graph_view_handle)view_ptr);

  if (g_current_view == view_ptr)
  {
    set_main_window_title((graph_view_handle)view_ptr);
  }

  menu_view_changed();
}

static void detach_canvas(struct graph_view * view_ptr)
{
  GtkWidget * child;

  child = gtk_bin_get_child(GTK_BIN(g_main_scrolledwin));
  if (child == view_ptr->canvas_widget)
  {
    gtk_container_remove(GTK_CONTAINER(g_main_scrolledwin), view_ptr->canvas_widget);
    g_current_view = NULL;
    gtk_widget_show(g_view_label);
    gtk_scrolled_window_add_with_viewport(g_main_scrolledwin, g_view_label);
  }
}

#define view_ptr ((struct graph_view *)context)

static void app_added(void * context, uint64_t id, const char * name, bool running, bool terminal, const char * level)
{
  //log_info("app added. id=%"PRIu64", name='%s', %srunning, %s, level '%s'", id, name, running ? "" : "not ", terminal ? "terminal" : "shell", level);
  world_tree_add_app(context, id, name, running, terminal, level);
}

static void app_state_changed(void * context, uint64_t id, const char * name, bool running, bool terminal, const char * level)
{
  //log_info("app state changed. id=%"PRIu64", name='%s', %srunning, %s, level '%s'", id, name, running ? "" : "not ", terminal ? "terminal" : "shell", level);
  world_tree_app_state_changed(context, id, name, running, terminal, level);
}

static void app_removed(void * context, uint64_t id)
{
  //log_info("app removed. id=%"PRIu64, id);
  world_tree_remove_app(context, id);
}

static
void
project_properties_changed(
  void * context,
  const char * project_dir,
  const char * project_name,
  const char * UNUSED(project_description),
  const char * UNUSED(project_notes))
{
  bool empty;
  char * project_name_buffer;
  char * full_name_buffer;

  log_info("Room '%s' project properties changed. name='%s', dir='%s'", view_ptr->view_name, project_name, project_dir);

  empty = strlen(project_name) == 0;

  if (!empty)
  {
    project_name_buffer = strdup(project_name);
    if (project_name_buffer == NULL)
    {
      return;
    }

    full_name_buffer = catdup4(view_ptr->view_name, " (", project_name, ")");
    if (full_name_buffer == NULL)
    {
      free(project_name_buffer);
      return;
    }

    if (view_ptr->full_name != NULL && view_ptr->full_name != view_ptr->view_name)
    {
      free(view_ptr->full_name);
    }

    if (view_ptr->project_name != NULL)
    {
      free(view_ptr->project_name);
    }

    view_ptr->full_name = full_name_buffer;
    view_ptr->project_name = project_name_buffer;
  }
  else
  {
    if (view_ptr->full_name != NULL && view_ptr->full_name != view_ptr->view_name)
    {
      free(view_ptr->full_name);
    }

    if (view_ptr->project_name != NULL)
    {
      free(view_ptr->project_name);
    }

    view_ptr->full_name = view_ptr->view_name;
    view_ptr->project_name = NULL;
  }

  announce_view_name_change(view_ptr);
}

#undef view_ptr

static void fill_canvas_menu(GtkMenu * menu)
{
  fill_view_popup_menu(menu, (graph_view_handle)g_current_view);
}

bool
create_view(
  const char * name,
  const char * service,
  const char * object,
  bool graph_dict_supported,
  bool graph_manager_supported,
  bool app_supervisor_supported,
  bool force_activate,
  graph_view_handle * handle_ptr)
{
  struct graph_view * view_ptr;

  view_ptr = malloc(sizeof(struct graph_view));
  if (view_ptr == NULL)
  {
    log_error("malloc() failed for struct graph_view");
    goto fail;
  }

  view_ptr->view_name = strdup(name);
  if (view_ptr->view_name == NULL)
  {
    log_error("strdup() failed for \"%s\"", name);
    goto free_view;
  }

  view_ptr->app_supervisor = NULL;
  view_ptr->room = NULL;
  view_ptr->full_name = view_ptr->view_name;
  view_ptr->project_name = NULL;

  if (!graph_proxy_create(service, object, graph_dict_supported, graph_manager_supported, &view_ptr->graph))
  {
    goto free_name;
  }

  if (!graph_canvas_create(1600 * 2, 1200 * 2, fill_canvas_menu, &view_ptr->graph_canvas))
  {
    goto destroy_graph;
  }

  if (!graph_canvas_attach(view_ptr->graph_canvas, view_ptr->graph))
  {
    goto destroy_graph_canvas;
  }

  view_ptr->canvas_widget = canvas_get_widget(graph_canvas_get_canvas(view_ptr->graph_canvas));

  list_add_tail(&view_ptr->siblings, &g_views);

  gtk_widget_show(view_ptr->canvas_widget);

  world_tree_add((graph_view_handle)view_ptr, force_activate);

  if (app_supervisor_supported)
  {
    if (!ladish_app_supervisor_proxy_create(service, object, view_ptr, app_added, app_state_changed, app_removed, &view_ptr->app_supervisor))
    {
      goto remove_from_world_tree;
    }
  }

  if (strcmp(object, STUDIO_OBJECT_PATH) != 0 && strcmp(object, JACKDBUS_OBJECT_PATH) != 0) /* TODO: this is a quite lame way to detect room views */
  {
    if (!ladish_room_proxy_create(service, object, view_ptr, project_properties_changed, &view_ptr->room))
    {
      goto free_app_supervisor;
    }
  }

  if (!graph_proxy_activate(view_ptr->graph))
  {
    goto free_room_proxy;
  }

  menu_view_changed();

  *handle_ptr = (graph_view_handle)view_ptr;

  return true;

free_room_proxy:
  if (view_ptr->room != NULL)
  {
    ladish_room_proxy_destroy(view_ptr->room);
  }
free_app_supervisor:
  if (view_ptr->app_supervisor != NULL)
  {
    ladish_app_supervisor_proxy_destroy(view_ptr->app_supervisor);
  }
remove_from_world_tree:
  list_del(&view_ptr->siblings);
  if (!list_empty(&g_views))
  {
    world_tree_activate((graph_view_handle)list_entry(g_views.next, struct graph_view, siblings));
  }
  else
  {
    set_main_window_title(NULL);
  }

  detach_canvas(view_ptr);

  world_tree_remove((graph_view_handle)view_ptr);
destroy_graph_canvas:
  graph_canvas_destroy(view_ptr->graph_canvas);
destroy_graph:
  graph_proxy_destroy(view_ptr->graph);
free_name:
  if (view_ptr->full_name != NULL && view_ptr->full_name != view_ptr->view_name)
  {
    free(view_ptr->full_name);
  }
  if (view_ptr->project_name != NULL)
  {
    free(view_ptr->project_name);
  }
  free(view_ptr->view_name);
free_view:
  free(view_ptr);
fail:
  return false;
}

static void attach_canvas(struct graph_view * view_ptr)
{
  GtkWidget * child;

  child = gtk_bin_get_child(GTK_BIN(g_main_scrolledwin));

  if (child == view_ptr->canvas_widget)
  {
    return;
  }

  if (child != NULL)
  {
    gtk_container_remove(GTK_CONTAINER(g_main_scrolledwin), child);
  }

  g_current_view = view_ptr;
  gtk_container_add(GTK_CONTAINER(g_main_scrolledwin), view_ptr->canvas_widget);

  //_main_scrolledwin->property_hadjustment().get_value()->set_step_increment(10);
  //_main_scrolledwin->property_vadjustment().get_value()->set_step_increment(10);
}

#define view_ptr ((struct graph_view *)view)

void destroy_view(graph_view_handle view)
{
  list_del(&view_ptr->siblings);
  if (!list_empty(&g_views))
  {
    world_tree_activate((graph_view_handle)list_entry(g_views.next, struct graph_view, siblings));
  }
  else
  {
    set_main_window_title(NULL);
  }

  detach_canvas(view_ptr);

  world_tree_remove(view);

  graph_canvas_detach(view_ptr->graph_canvas);
  graph_canvas_destroy(view_ptr->graph_canvas);
  graph_proxy_destroy(view_ptr->graph);

  if (view_ptr->app_supervisor != NULL)
  {
    ladish_app_supervisor_proxy_destroy(view_ptr->app_supervisor);
  }

  if (view_ptr->room != NULL)
  {
    ladish_room_proxy_destroy(view_ptr->room);
  }

  if (view_ptr->full_name != NULL && view_ptr->full_name != view_ptr->view_name)
  {
    free(view_ptr->full_name);
  }
  if (view_ptr->project_name != NULL)
  {
    free(view_ptr->project_name);
  }
  free(view_ptr->view_name);

  free(view_ptr);
}

void activate_view(graph_view_handle view)
{
  attach_canvas(view_ptr);
  set_main_window_title(view);
  menu_view_changed();
}

const char * get_view_name(graph_view_handle view)
{
  return view_ptr->full_name;
}

const char * get_view_opath(graph_view_handle view)
{
  return graph_proxy_get_object(view_ptr->graph);
}

bool set_view_name(graph_view_handle view, const char * name)
{
  char * view_name_buffer;
  char * full_name_buffer;

  view_name_buffer = strdup(name);
  if (view_name_buffer == NULL)
  {
    log_error("strdup() failed for \"%s\"", name);
    return false;
  }

  if (view_ptr->project_name != NULL)
  {
    full_name_buffer = catdup4(view_name_buffer, " (", view_ptr->project_name, ")");
    if (full_name_buffer == NULL)
    {
      free(view_name_buffer);
      return false;
    }
  }
  else
  {
    full_name_buffer = view_name_buffer;
  }

  if (view_ptr->full_name != NULL && view_ptr->full_name != view_ptr->view_name)
  {
    free(view_ptr->full_name);
  }

  free(view_ptr->view_name);

  view_ptr->view_name = view_name_buffer;
  view_ptr->full_name = full_name_buffer;

  announce_view_name_change(view_ptr);

  return true;
}

canvas_handle get_current_canvas(void)
{
  if (g_current_view == NULL)
  {
    return NULL;
  }

  return graph_canvas_get_canvas(g_current_view->graph_canvas);
}

const char * get_current_view_room_name(void)
{
  if (g_current_view == NULL || !is_room_view((graph_view_handle)g_current_view))
  {
    return NULL;
  }

  return g_current_view->view_name;
}

graph_view_handle get_current_view(void)
{
  return (graph_view_handle)g_current_view;
}

bool is_room_view(graph_view_handle view)
{
  return view_ptr->room != NULL;
}

bool app_run_custom(graph_view_handle view, const char * command, const char * name, bool run_in_terminal, const char * level)
{
  return ladish_app_supervisor_proxy_run_custom(view_ptr->app_supervisor, command, name, run_in_terminal, level);
}

ladish_app_supervisor_proxy_handle graph_view_get_app_supervisor(graph_view_handle view)
{
  return view_ptr->app_supervisor;
}

ladish_room_proxy_handle graph_view_get_room(graph_view_handle view)
{
  return view_ptr->room;
}

bool room_has_project(graph_view_handle view)
{
  return view_ptr->project_name != NULL;
}
