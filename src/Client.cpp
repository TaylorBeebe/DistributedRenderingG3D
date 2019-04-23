#include "Node.h"

using namespace std;
using namespace G3D;
using namespace DistributedRenderer;

namespace DistributedRenderer{

    Client::Client(RApp& app) : NetworkNode(NodeType::CLIENT, Constants::ROUTER_ADDR, app, false) {}

    void onConnect() {
        // send router intoduction
        send(PacketType::HI_AM_CLIENT);

        bool server_online = false;

        // wait for an ACK
        // then wait for a ready
        while (true) {
            for (NetMessageIterator& iter = connection->incomingMessageIterator(); iter.isValid(); iter++){
                switch(iter.type()){
                    case PacketType::ACK:
                        server_online = true;
                        break;
                    case PacketType::READY:
                        running = true;
                        return;
                    default: break;
                }
            }
        }
    }

    void Client::checkNetwork(){

        NetMessageIterator& iter = connection->incomingMessageIterator();

        if(!iter.isValid()) return;

        try{

            BinaryInput& header = iter.headerBinaryInput();
            header.beginBits();
            uint32 batch_id = header.readUInt32();

			shared_ptr<Image> frame;

            switch(iter.type()){
                case PacketType::FRAME:

					frame = Image::fromBinaryInput(iter.binaryInput(), ImageFormat::RGB8());

                    // cache in frame buffer

                    break;
                case PacketType::TERMINATE:
                    // clean up
                    running = false;
                    break;
                default: // Client does not need this datatype
                    debugPrintf("Client received incompatible packet type\n");
                    break;   
            } // end switch

            header.endBits();

        } catch(...) {
            // handle error
        }

        iter++;
    }

    // Use this method to mark an entity to be updated on the network
    void Client::setEntityChanged(uint32 id){
        // safety check
        changed_entities.insert(id);
    }

	// send an update on the network with a batch ID
	// the processed batch frame will need to return by the next deadline
	// or else the client will use a low qual render instead
    void Client::sendUpdate(){

        current_batch_id++;
        ms_to_deadline = 1000 / Constants::FRAMERATE;

        // serialize 
        BinaryOutput& batch = BinaryUtils::empty();

        batch.beginBits();

        // this currently loops through every entity
        // this is inefficient and should be improved such that we only iterate through a 
        // set of ids that were changed
        for (map<unsigned int, Entity*>::iterator it = entityRegistry.begin(); it!=entityRegistry.end(); ++it){
            Entity* ent = it->second;

            float x,y,z,yaw,pitch,roll;
            ent->frame().getXYZYPRRadians(x,y,z,yaw,pitch,roll);

            batch.writeUInt32(it->first);
			batch.writeFloat32(x);
			batch.writeFloat32(y);
			batch.writeFloat32(z);
			batch.writeFloat32(yaw);
			batch.writeFloat32(pitch);
			batch.writeFloat32(roll);

        }

        batch.endBits();

        // net message send batch to router ip
        send(PacketType::UPDATE, BinaryUtils::toBinaryOutput(current_batch_id), batch);

        // clear recently used
        // changed_entities.erase();
    }
}
