/* This file is part of Ingen.  Copyright (C) 2006 Dave Robillard.
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef NEWSUBPATCHWINDOW_H
#define NEWSUBPATCHWINDOW_H

#include "PluginModel.h"
#include <libglademm/xml.h>
#include <gtkmm.h>
#include "util/CountedPtr.h"
#include "PatchModel.h"
using Ingen::Client::PatchModel;
using Ingen::Client::MetadataMap;

namespace Ingenuity {
	

/** 'New Subpatch' window.
 *
 * Loaded by glade as a derived object.
 *
 * \ingroup Ingenuity
 */
class NewSubpatchWindow : public Gtk::Window
{
public:
	NewSubpatchWindow(BaseObjectType* cobject, const Glib::RefPtr<Gnome::Glade::Xml>& xml);

	void set_patch(CountedPtr<PatchModel> patch);

	void present(CountedPtr<PatchModel> patch, MetadataMap data);

private:
	void name_changed();
	void ok_clicked();
	void cancel_clicked();

	MetadataMap            m_initial_data;
	CountedPtr<PatchModel> m_patch;
	
	double m_new_module_x;
	double m_new_module_y;
	
	Gtk::Entry*      m_name_entry;
	Gtk::Label*      m_message_label;
	Gtk::SpinButton* m_poly_spinbutton;
	Gtk::Button*     m_ok_button;
	Gtk::Button*     m_cancel_button;
};
 

} // namespace Ingenuity

#endif // NEWSUBPATCHWINDOW_H
