#pragma once
#include <string.h>
#include <map>

namespace RemoteRenderer{

	unsigned int FRAMERATE = 30

	string SERVER = "0.0.0.0"
	string CLIENT = "0.0.0.0"
	string NODE1 = "0.0.0.0"
	string NODE2 = "0.0.0.0"
	string NODE3 = "0.0.0.0"

	typedef struct {
	    bool ref; // dirty bit
	    Entity* entity;
	} network_entity;

	bool isClient = true;
	bool isServer = true;
	bool isRemote = true;

	// Each entity in the scene will have a registered network ID
	// which will be synced across the network at setup so that at 
	// runtime transform data can be synced 
	map<unsigned int, network_entity*> entityRegistry;
	
	unsigned int net_nonce = 0;

	// @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
    // @post: creates network ID for entity and stores reference to it
    // @returns: network ID of entity 
    unsigned int registerEntity(Entity* e){
        network_entity* entity = new network_entity();
        entity->ref = false;
        entity->entity = e;

        entityRegistry[net_nonce] = entity;

        return net_nonce++;
    } 

}