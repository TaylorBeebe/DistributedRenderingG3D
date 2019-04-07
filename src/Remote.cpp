#include "Node.h"

using namespace DistributedRenderer;

namespace DistributedRenderer{

    Remote::Remote(RApp& app, bool headless_mode) : NetworkNode(Constants::ROUTER_ADDR, NodeType::REMOTE, app), headless(headless_mode) {}


    void Remote::setClip(uint y, uint height){
        bounds = Rect2D::xywh(0,y,Constants::SCREEN_WIDTH,height);
    }

    void Remote::receive() {

        NetMessageIterator& iter = connection->incomingMessageIterator();
        BinaryInput header;
        uint batch_id;

        if(!iter.isValid()) return;
        
        try{
            // read the header
            header = iter.headerBinaryInput();
            header.beginBits();

            batch_id = header.readUInt32();

            switch(iter.type()){
                case PacketType::UPDATE: // update data
                    sync(iter.binaryInput());
                    // the_app.oneFrameAdHoc();
                    sendFrame(batch_id);
                    break;

                case PacketType::CONFIG: // router delivers the dimension data for this node and awaits reply
                    // unpack screen data
                    BinaryInput& bi = iter.binaryInput();
                    bi.beginBits();
                    uint y = bi.readUInt32();
                    uint h = bi.readUInt32();
                    bi.endBits();

                    setBounds(y, h);
                    
                    send(PacketType::CONFIG_RECEIPT);

                case PacketType::READY: // client application will start, just chill
                    running = true;
                    break;

                case PacketType::TERMINATE: // this is the end of all messages
                    // clean up app
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

    // @pre: transform packet with list of transforms of entities to update
    // @post: updates frame of corresponding entity with new position data
    void Remote::sync(BinaryInput& update){
        update.beginBits();

        while(update.hasMore()){
            uint id = update.readUInt32();
            float x = update.readFloat32();
            float y = update.readFloat32();
            float z = update.readFloat32();
            float yaw = update.readFloat32();
            float pitch = update.readFloat32();
            float roll = update.readFloat32();

            entityRegistry[id]->getframe()->fromXYZYPRRadians(x,y,z,yaw,pitch,roll);
        }

        update.endBits();
    }

    // @pre: the current batch id
    // @post: renders a new frame and sends it in a frame packet back to the router
    void Remote::sendFrame(uint batch_id){

        // maybe spawn a new thread to do it async and if another packet comes in stop
        // that thread and spawn a new one

        // call to renderer here ...

        // store it in packet and send it
    }
}
