#pragma once
#include <map>
#include <G3D/G3D.h>

namespace RemoteRenderer{

	unsigned int FRAMERATE = 30

	bool isClient = false;
	bool isServer = false;
	bool isRemote = false;

	G3D::NetAddress SERVER;

	// Each entity in the scene will have a registered network ID
	// which will be synced across the network at setup so that at 
	// runtime transform data can be synced 
	map<unsigned int, G3D::Entity*> entityRegistry;
	
	unsigned int net_nonce = 0;

	// @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
    // @post: creates network ID for entity and stores reference to it
    // @returns: network ID of entity 
    unsigned int registerEntity(Entity* e){
        entityRegistry[net_nonce] = e;
        return net_nonce++;
    } 

    enum NetMessageType {
		TRANSFORM,
		FRAME,
		FRAME_FRAG
	}

}