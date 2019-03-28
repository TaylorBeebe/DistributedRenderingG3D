#include "Client.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

// @pre: incoming data packet
// @post: saves the frame and renders it on the dealine (hopefully)
void Client::onData(uint socket_id, RenderPacket* packet){

    switch(packet.getPacketType()){
        case FRAME:
            // TODO: what will the client do when it receives a frame
            break;    
        default:
            debugPrintf("Client received incompatible packet type\n");
            break;   
    }
}

// @pre: registered ID of an entity
// @post: marks that entity as changed
void Client::setEntityChanged(unsigned int id){
    // safety check
    changed_entities.insert(id);
}

// @pre: expects a deadline in ms that will be used as reference by the network
// @post: sends updated entity data on the network with corresponding job ID and deadline
void Client::renderOnNetwork(){

    current_job_id++;
    ms_to_deadline = 1000 / FRAMERATE;

    // initialize batch
    TransformPacket* batch = new TransformPacket(current_job_id);

    // this currently loops through every entity
    // this is inefficient and should be improved such that we only iterate through a 
    // set of ids that were changed
    for (map<unsigned int, Entity*>::iterator it = entityRegistry.begin(); it!=entityRegistry.end(); ++it){
        Enitity* ent = it->second;

        batch->addTransform(it->first, ent->frame);
    }

    // net message send batch to server ip
    send(batch);

    // clear recently used
    changed_entities.erase();
}


}
