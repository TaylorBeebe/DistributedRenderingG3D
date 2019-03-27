#include "Client.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

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
    BinaryOutput* batch = new G3D::BinaryOutput();
    batch->setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

    batch->beginBits();

    // this currently loops through every entity
    // this is inefficient and should be improved such that we only iterate through a 
    // set of ids that were changed
    for (map<unsigned int, Entity*>::iterator it = entityRegistry.begin(); it!=entityRegistry.end(); ++it){
        Enitity* ent = it->second;

        float x,y,z,y,p,r;
        ent->entity->frame->getXYZYPRRadians(x,y,z,y,p,r)

        // add x,y,z,y,p,r to batch
        
    }

    batch->endBits();

    // net message send batch to server ip
    send(batch);

    // clear recently used
    // changed_entities.erase();
}


}
