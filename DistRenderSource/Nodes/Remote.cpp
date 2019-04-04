#include "Node.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{

    Remote::Remote() : NetworkNode(Constants::ROUTER_ADDR, NodeType::REMOTE) {}

    void Remote::setBounds(float sx, float sy, float ex, float ey){
        bounds = G3D::Rect2D::xyxy(sx,sy,ex,ey);
    }

    // continuosly listen for updates and render them accordingly
    void Remote::receive() {

        G3D::NetMessageIterator& iter = connection->incomingMessageIterator();

        G3D::BinaryInput& header;
        uint batch_id;

        while(1){
            // busy wait until there's a message (room for opitimization with thread sleeping but only with a custom networking model)
            // network listeners run on a separate network thread and will add to the iter queue in a threadsafe fashion
            if(!iter.isValid()) continue;
            
            try{
                // read the header
                header = iter.headerBinaryInput();
                header.beginBits();

                batch_id = header.readUInt32();

                switch(iter.type()){
                    case PacketType::TRANSFORM: // update data
                        syncTransforms(iter.binaryInput());
                        sendFrame(batch_id);
                        break;

                    case PacketType::HANDSHAKE: // router delivers the dimension data for this node and awaits reply
                        // unpack screen data
                        // setBounds(sx,sy,ex,ey);
                        received_screen_data = true;
                        maybeRespondHandshake();

                    case PacketType::READY: // client application will start, just chill
                        running = true;
                        break;

                    case PacketType::END: // this is the end of all messages
                        // clean up
                        break;

                    default: // Remote Node does not need this datatype
                        debugPrintf("Remote Node received incompatible packet type\n");

                } // end switch

                header.endBits();

            } catch(...) { // something went wrong decoding the message
                // handle error or do nothing
            }

            // pop the message off of the queue
            ++iter;
        }
    }

    // @pre: transform packet with list of transforms of entities to update
    // @post: updates frame of corresponding entity with new position data
    void Remote::syncTransforms(G3D::BinaryInput& transforms){
        transforms.beginBits();

        while(transforms.hasMore()){
            uint id = transforms.readUInt32();
            float x = transforms.readFloat32();
            float y = transforms.readFloat32();
            float z = transforms.readFloat32();
            float yaw = transforms.readFloat32();
            float pitch = transforms.readFloat32();
            float roll = transforms.readFloat32();

            entityRegistry[id]->getframe()->fromXYZYPRRadians(x,y,z,yaw,pitch,roll);
        }

        transforms.endBits();
    }

    // @pre: the current batch id
    // @post: renders a new frame and sends it in a frame packet back to the router
    void Remote::sendFrame(uint batch_id){

        // maybe spawn a new thread to do it async and if another packet comes in stop
        // that thread and spawn a new one

        // call to renderer here ...

        // store it in packet and send it
    }

    void Remote::finishedSetup(){
        finished_setup = true;
        maybeRespondHandshake();
    }

    void Remote::maybeRespondHandshake(){
        if(received_screen_data && finished_setup){
            send(PacketType::HANDSHAKE);
        }
    }

}
