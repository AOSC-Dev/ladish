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

#ifndef OBJECTMODEL_H
#define OBJECTMODEL_H

#include <cstdlib>
#include <map>
#include <iostream>
#include <string>
#include <algorithm>
#include <cassert>
#include <sigc++/sigc++.h>
#include "util/Atom.h"
#include "util/Path.h"
#include "util/CountedPtr.h"
using std::string; using std::map; using std::find;
using std::cout; using std::cerr; using std::endl;

namespace Ingen {
namespace Client {

typedef map<string, Atom> MetadataMap;


/** Base class for all GraphObject models (NodeModel, PatchModel, PortModel).
 *
 * There are no non-const public methods intentionally, models are not allowed
 * to be manipulated directly by anything (but the Store) because of the
 * asynchronous nature of engine control.  To change something, use the
 * controller (which the model probably shouldn't have a reference to but oh
 * well, it reduces Collection Hell) and wait for the result (as a signal
 * from this Model).
 *
 * \ingroup IngenClient
 */
class ObjectModel
{
public:
	ObjectModel(const Path& path);

	virtual ~ObjectModel();

	const Atom& get_metadata(const string& key) const;
	void        add_metadata(const MetadataMap& data);

	const MetadataMap&      metadata() const { return _metadata; }
	inline const Path&      path()     const { return _path; }
	CountedPtr<ObjectModel> parent()   const { return _parent; }

	void assimilate(CountedPtr<ObjectModel> model);

	// Signals
	sigc::signal<void, const string&, const Atom&> metadata_update_sig; 
	sigc::signal<void>                             destroyed_sig; 
	
	// FIXME: make private
	void set_metadata(const string& key, const Atom& value)
		{ _metadata[key] = value; metadata_update_sig.emit(key, value); }


protected:
	friend class Store;
	friend class PatchLibrarian; // FIXME: remove
	virtual void set_path(const Path& p)               { _path = p; }
	virtual void set_parent(CountedPtr<ObjectModel> p) { _parent = p; }
	virtual void add_child(CountedPtr<ObjectModel> c) = 0;
	virtual void remove_child(CountedPtr<ObjectModel> c) = 0;

	Path                         _path;
	CountedPtr<ObjectModel>      _parent;
	
	MetadataMap _metadata;

private:
	// Prevent copies (undefined)
	ObjectModel(const ObjectModel& copy);
	ObjectModel& operator=(const ObjectModel& copy);
};


} // namespace Client
} // namespace Ingen

#endif // OBJECTMODEL_H
