#include "Node.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{

    Remote::Remote() : NetworkNode(constants::ROUTER_ADDR, NodeType::REMOTE) {}

    void Remote::setBounds(float sx, float sy, float ex, float ey){
        bounds = G3D::Rect2D::xyxy(sx,sy,ex,ey);
    }

    void waitForNextUpdate() {
        // busy wait until there's a message (room for opitimization with thread sleeping but only with a custom networking model)
        // network listeners run on a separate network thread and will add to the iter queue in a threadsafe fashion
        while(!iter.isValid()) {}

        readNetworkUpdate();
    }


    // @pre: encoded batch of transform updates
    // @post: updates corresponding entity transforms and triggers a frame render
    void Remote::onData(RenderPacket& packet){
        switch(packet.getPacketType()){
            case PacketType::TRANSFORM:
                // update with the data
                syncTransforms(packet);
                // render a frame with the update
                sendFrame(packet.getBatchId());
                
                break;
            case PacketType::HANDSHAKE:
                // unpack screen data
                // setBounds(sx,sy,ex,ey);
                received_screen_data = true;
                maybeRespondHandshake();
            case PacketType::READY:
                // application will start, just chill
                running = true;
                break;
            case PacketType::END:
                // clean up
                break;
            default:
                // Remote Node does not need this datatype
                debugPrintf("Received useless packet");
                break;
        }
    }

    // @pre: transform packet with list of transforms of entities to update
    // @post: updates frame of corresponding entity with new position data
    void Remote::syncTransforms(RenderPacket& packet){

        try{
            G3D::BinaryInput message = packet.getBody();

            message.beginBits();

            while(message.hasMore()){
                uint id = message.readUInt32();
                float x = message.readFloat32();
                float y = message.readFloat32();
                float z = message.readFloat32();
                float yaw = message.readFloat32();
                float pitch = message.readFloat32();
                float roll = message.readFloat32();

                entityRegistry[id]->getframe()->fromXYZYPRRadians(x,y,z,yaw,pitch,roll);
            }

            message.endBits();

            // destroy message

        }catch(...){
            // something bad happened, badly formed message
            // toss out and let deadline pass
        }
    }

    // @pre: the current batch id
    // @post: renders a new frame and sends it in a frame packet back to the router
    void Remote::sendFrame(uint batch_id){

        // maybe spawn a new thread to do it async and if another packet comes in stop
        // that thread and spawn a new one

        // call to renderer here ...

        // store it in packet and send it

        RenderPacket packet (FRAME, batch_id);
        send(packet);
    }

    void Remote::maybeRespondHandshake(){
        if(received_screen_data && finished_setup){
            send(PacketType::HANDSHAKE);
        }
    }

}
