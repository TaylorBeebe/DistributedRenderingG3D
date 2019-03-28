#include "RemoteNode.h"
#include <list>

using namespace RemoteRenderer;

namespace RemoteRenderer{

// @pre: encoded batch of transform updates
// @post: updates corresponding entity transforms and triggers a frame render
void RemoteNode::onData(uint socket_id, RenderPacket* packet){
    switch(packet->getPacketType()){
        case FRAME:
            // Remote Node does not need frames
            // printfDebug("Remote Node received frame instead of transform");
            break;
        case TRANSFORM:

            std::list<transform_t*> transforms = ((TransformPacket*) packet)->transforms;

            for (std::list<transform_t*>::iterator it= transforms.begin(); it != transforms.end(); ++it){
                syncEntityTransform(*it)
            }

            sendFrameSegment(packet->getBatchId());
            break;
    }
}

// @pre: registered ID and frame data of an entity
// @post: updates frame of corresponding entity with new position data
void RemoteNode::syncEntityTransform(transform_t* t){
    // safety check
    entityRegistry[id]->frame->fromXYZYPRRadians(t->x, t->y, t->z, t->yaw, t->pitch, t->roll);
}

// @pre: the current batch id
// @post: renders a new frame and sends it in a frame packet back to the server
void RemoteNode::sendFrameSegment(uint batch_id){
    FramePacket* packet = new FramePacket(batch_id, width, height);

    // render something 
    // store it in packet

    // send it
    send(packet);

}



}
