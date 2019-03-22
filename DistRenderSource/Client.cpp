#include "Client.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

// @pre: registered ID of an entity
// @post: sets the dirty bit of that entity
void Client::setDirtyBit(unsigned int id){
    // safety check
    entities[id]->ref = true;
}

// @pre: expects a deadline in ms that will be used as reference by the network
// @post: sends updated entity data on the network with corresponding job ID and deadline
void Client::renderOnNetwork(){

    current_job++;
    ms_to_deadline = 1000 / FRAMERATE;

    // initialize batch

    for (map<unsigned int, network_entity*>::iterator it = entityRegistry.begin(); it!=entityRegistry.end(); ++it){
        network_entity* ent = it->second;
        if(ent->ref){
            ent->ref = false;

            float x,y,z,y,p,r;
            ent->entity->frame->getXYZYPRRadians(x,y,z,y,p,r)

            // add x,y,z,y,p,r to batch
        }
    }

    // net message send batch to server ip

}

}
