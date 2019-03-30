#include "Remote.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{

    Remote::Remote(int sx, int sy, int ex, int ey) : Node::SingleConnectionNode(REMOTE), 
                                                     startX(sx), 
                                                     startY(sy), 
                                                     endX(ex), 
                                                     endY(ey),
                                                     width(ex-sx),
                                                     height(ey-sy) {}

    // @pre: encoded batch of transform updates
    // @post: updates corresponding entity transforms and triggers a frame render
    void Remote::onData(uint socket_id, RenderPacket* packet){
        switch(packet->getPacketType()){
            case FRAME:
                // Remote Node does not need frames
                // printfDebug("Remote Node received frame instead of transform");
                break;
            case TRANSFORM:
                // decode the data
                TransformPacket* trasform_data = new TransformPacket(packet);
                
                // update with the data
                syncTransforms(transform_data);

                // render a frame with the update
                renderFrameSegment(packet->getBatchId());
                break;
        }
    }

    // @pre: transform packet with list of transforms of entities to update
    // @post: updates frame of corresponding entity with new position data
    void Remote::syncTransforms(TransformPacket* packet){
        std::list<transform_t*> transforms = packet->getTransforms();

        for (std::list<transform_t*>::iterator it = transforms.begin(); it != transforms.end(); ++it){
            transform_t t = *it;
            // safety check
            entityRegistry[t.id]->frame->fromXYZYPRRadians(t.x, t.y, t.z, t.yaw, t.pitch, t.roll);
        }
    }

    // @pre: the current batch id
    // @post: renders a new frame and sends it in a frame packet back to the server
    void Remote::renderFrameSegment(uint batch_id){

        // render something
        // maybe spawn a new thread to do it async and if another packet comes in stop
        // that thread and spawn a new one

        // store it in packet and send it
        FramePacket* packet = new FramePacket(batch_id, width, height);
        send(packet);
    }

}
