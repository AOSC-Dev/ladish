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

#include "interface/ClientInterface.h"
#include "interface/ClientKey.h"
#include "OSCModelEngineInterface.h"
#include "PatchLibrarian.h"
#include "DemolitionClientInterface.h"
#include "util/CountedPtr.h"
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include "cmdline.h"  // generated by gengetopt

using std::cout;
using std::endl;

using namespace Ingen::Client;

void do_something();

string random_name();

void create_patch(); 
void destroy(); 
void add_node(); 
void connect(); 
void disconnect(); 
void disconnect_all(); 
void set_port_value(); 
void set_port_value_queued(); 
void rename_object();


// Yay globals!
DemolitionModel* model;
OSCModelEngineInterface*   engine;
	

int
main(int argc, char** argv)
{
	const char* engine_url  = NULL;
	int         client_port = 0;

	/* Parse command line options */
	gengetopt_args_info args_info;
	if (cmdline_parser (argc, argv, &args_info) != 0)
		return 1;

	if (args_info.engine_url_given) {
		engine_url = args_info.engine_url_arg;
	} else {
		cout << "[Main] No engine URL specified.  Attempting to use osc.udp://localhost:16180" << endl;
		engine_url = "osc.udp://localhost:16180";
	}	

	if (args_info.client_port_given)
		client_port = args_info.client_port_arg;
	else
		client_port = 0; // will choose a free port automatically
	
	model   = new DemolitionModel();
	
	// Create this first so engine interface (liblo) uses the port
	CountedPtr<DemolitionClientInterface> client
		= CountedPtr<DemolitionClientInterface>(new DemolitionClientInterface(model));
	
	engine = new OSCModelEngineInterface(engine_url);
	engine->activate();
	
	/* Connect to engine */
	//engine->attach(engine_url);
	engine->register_client(ClientKey(), (CountedPtr<ClientInterface>)client);
	
	engine->load_plugins();
	engine->request_plugins();

	//int id = engine->get_next_request_id();
	engine->request_all_objects(/*id*/);
	//engine->set_wait_response_id(id);
	//engine->wait_for_response();

	// Disable DSP for stress testing
	engine->disable_patch("/");
	
	while (true) {
		do_something();
		usleep(10000);
	}
	
	sleep(2);
	engine->unregister_client(ClientKey());
	//engine->detach();

	delete engine;
	delete model;
	
	return 0;
}

/** Does some random action
 */
void
do_something()
{
	int action = rand() % 10;

	switch(action) {
	case 0:
		create_patch(); break;
	case 1:
		destroy(); break;
	case 2:
		add_node(); break;
	case 3:
		connect(); break;
	case 4:
		disconnect(); break;
	case 5:
		disconnect_all(); break;
	case 6:
		set_port_value(); break;
	case 7:
		set_port_value_queued(); break;
	case 8:
		rename_object(); break;
	default:
		break;
	}
}


string
random_name()
{
	int length = (rand()%10)+1;
	string name(length, '-');

	for (int i=0; i < length; ++i)
		name[i] = 'a' + rand()%26;

	return name;
}


void
create_patch()
{
	// Make the probability of this happening inversely proportionate to the number
	// of patches to keep the # in a sane range
	//if (model->num_patches() > 0 && (rand() % model->num_patches()))
	//	return;

	bool subpatch = rand()%2;
	PatchModel* parent = NULL;
	string name = random_name();
	PatchModel* pm = NULL;
	
	if (subpatch)
		parent = model->random_patch();

	if (parent != NULL)
		pm = new PatchModel(parent->path() +"/"+ name, (rand()%8)+1);
	else
		pm = new PatchModel(string("/") + name, (rand()%8)+1);

	cout << "Creating patch " << pm->path() << endl;

	engine->create_patch_from_model(pm);
	
	// Spread them out a bit for easier monitoring with ingenuity
	engine->set_metadata(pm->path(), "module-x", 1600 + rand()%800 - 400);
	engine->set_metadata(pm->path(), "module-y", 1200 + rand()%700 - 350);
	
	delete pm;
}

 
void
destroy()
{
	// Make the probability of this happening proportionate to the number
	// of patches to keep the # in a sane range
	if (model->num_patches() == 0 || !(rand() % model->num_patches()))
		return;

	NodeModel* nm = NULL;
	
	if (rand()%2)
		nm = model->random_patch();
	else
		nm = model->random_node();
		
	if (nm != NULL) {
		cout << "Destroying " << nm->path() << endl;
		engine->destroy(nm->path());
	}
}

 
void
add_node()
{
	PatchModel*  parent = model->random_patch();
	PluginModel* plugin = model->random_plugin();
	
	if (parent != NULL && plugin != NULL) {
		NodeModel* nm = new NodeModel(plugin, parent->path() +"/"+ random_name());
		cout << "Adding node " << nm->path() << endl;
		engine->create_node_from_model(nm);
	
		// Spread them out a bit for easier monitoring with ingenuity
		engine->set_metadata(pm->path(), "module-x", 1600 + rand()%800 - 400);
		engine->set_metadata(pm->path(), "module-y", 1200 + rand()%700 - 350);
	}
}

 
void
connect()
{
	if (!(rand() % 10)) {  // Attempt a connection between two nodes in the same patch
		PatchModel* parent = model->random_patch();
		NodeModel* n1 = model->random_node_in_patch(parent);
		NodeModel* n2 = model->random_node_in_patch(parent);
		PortModel* p1 = model->random_port_in_node(n1);
		PortModel* p2 = model->random_port_in_node(n2);
		
		if (p1 != NULL && p2 != NULL) {
			cout << "Connecting " << p1->path() << " -> " << p2->path() << endl;
			engine->connect(p1->path(), p2->path());
		}

	} else {  // Attempt a connection between two truly random nodes
		PortModel* p1 = model->random_port();
		PortModel* p2 = model->random_port();
	
		if (p1 != NULL && p2 != NULL) {
			cout << "Connecting " << p1->path() << " -> " << p2->path() << endl;
			engine->connect(p1->path(), p2->path());
		}
	}	
}

 
void
disconnect()
{
	PortModel* p1 = model->random_port();
	PortModel* p2 = model->random_port();

	if (p1 != NULL && p2 != NULL) {
		cout << "Disconnecting " << p1->path() << " -> " << p2->path() << endl;
		engine->disconnect(p1->path(), p2->path());
	}
}

 
void
disconnect_all()
{
	PortModel* p = model->random_port();

	if (p != NULL) {
		cout << "Disconnecting all from" << p->path() << endl;
		engine->disconnect_all(p->path());
	}
}

 
void
set_port_value()
{
	PortModel* pm = model->random_port();
	float val = (float)rand() / (float)RAND_MAX;
	
	if (pm != NULL) {
		cout << "Setting control for port " << pm->path() << " to " << val << endl;
		engine->set_port_value(pm->path(), val);
	}
}

 
void
set_port_value_queued()
{
	PortModel* pm = model->random_port();
	float val = (float)rand() / (float)RAND_MAX;
	
	if (pm != NULL) {
		cout << "Setting control (slow) for port " << pm->path() << " to " << val << endl;
		engine->set_port_value_queued(pm->path(), val);
	}
}


void
rename_object()
{
	// 1/6th chance of it being a patch
	/*int type = rand()%6;

	if (type == 0) {
		NodeModel* n = model->random_node();
		if (n != NULL)
			engine->rename(n->path(), random_name());
	} else {
		PatchModel* p = model->random_patch();
		if (p != NULL)
			engine->rename(p->path(), random_name());
	}*/
}
 
