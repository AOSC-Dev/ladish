/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2008, 2009, 2010, 2011 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains the interface of the world tree widget
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

#ifndef WORLD_TREE_H__D786489B_E400_4E92_85C7_2BAE606DE56D__INCLUDED
#define WORLD_TREE_H__D786489B_E400_4E92_85C7_2BAE606DE56D__INCLUDED

#include "graph_view.h"

void world_tree_init(void);

void world_tree_add(graph_view_handle view, bool force_activate);
void world_tree_remove(graph_view_handle view);
graph_view_handle world_tree_find_by_opath(const char * opath);
void world_tree_activate(graph_view_handle view);
void world_tree_name_changed(graph_view_handle view);
void world_tree_add_app(graph_view_handle view, uint64_t id, const char * name, bool running, bool terminal, const char * level);
void world_tree_app_state_changed(graph_view_handle view, uint64_t id, const char * name, bool running, bool terminal, const char * level);
void world_tree_remove_app(graph_view_handle view, uint64_t id);
void world_tree_destroy_room_views(void);

#endif // #ifndef WORLD_TREE_H__D786489B_E400_4E92_85C7_2BAE606DE56D__INCLUDED
