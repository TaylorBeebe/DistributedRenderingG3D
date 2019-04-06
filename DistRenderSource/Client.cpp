#include "Node.h"

using namespace std;
using namespace G3D;

namespace RemoteRenderer{

    Client::Client() : NetworkNode(Constants::ROUTER_ADDR, NodeType::CLIENT, RApp& app) {}

    void Client::checkNetwork(){

        NetMessageIterator& iter = connection->incomingMessageIterator();

        if(!iter.isValid()) return;

        try{

            G3D::BinaryInput& header = iter.headerBinaryInput();
            header.beginBits();
            uint batch_id = header.readUInt32();

            switch(iter.type()){
                case PacketType::FRAME:

                    shared_ptr<G3D::Image> frame = G3D::Image::fromBinaryInput(iter.binaryInput(), new ImageFormat::RGB8())

                    // cache in frame buffer

                    break;
                case PacketType::READY:
                    // if the client receives a ready from the router, this means that the router
                    // has contact with all nodes and is ready to start 

                    running = true;
                    
                    // start game tick

                    break;
                case PacketType::TERMINATE:
                    // clean up
                    break;
                default: // Client does not need this datatype
                    debugPrintf("Client received incompatible packet type\n");
                    break;   
            } // end switch

            header.endBits();

        } catch(...) {
            // handle error
        }

        ++iter;
    }

    // @pre: registered ID of an entity
    // @post: marks that entity as changed
    void Client::setEntityChanged(unsigned int id){
        // safety check
        changed_entities.insert(id);
    }

    // @pre: expects a deadline in ms that will be used as reference by the network
    // @post: sends updated entity data on the network with corresponding job ID and deadline
    void Client::sendTransforms(){

        current_batch_id++;
        ms_to_deadline = 1000 / FRAMERATE;

        // serialize 
        G3D::BinaryOutput batch ();
        batch.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

        batch.beginBits();

        // this currently loops through every entity
        // this is inefficient and should be improved such that we only iterate through a 
        // set of ids that were changed
        for (map<unsigned int, Entity*>::iterator it = entityRegistry.begin(); it!=entityRegistry.end(); ++it){
            Enitity* ent = it->second;

            float x,y,z,yaw,pitch,roll;
            ent->getFrame()->getXYZYPRRadians(x,y,z,yaw,pitch,roll);

            bitstream.writeUInt32(it->first);
            bitstream.writeFloat32(x);
            bitstream.writeFloat32(y);
            bitstream.writeFloat32(z);
            bitstream.writeFloat32(yaw);
            bitstream.writeFloat32(pitch);
            bitstream.writeFloat32(roll);

        }

        batch.endBits();

        // net message send batch to router ip
        send(TRANSFORM, BinaryUtils.toBinaryOutput(current_batch_id), batch);

        // clear recently used
        // changed_entities.erase();
    }
}
