#include "DistributedRenderer.h"

using namespace std;
using namespace G3D;
using namespace DistributedRenderer;

namespace DistributedRenderer{

    Client::Client(RApp* app) : NetworkNode(NodeType::CLIENT, app, false) {}

    void Client::onConnect() {
		
		cout << "Connected to router" << endl;

        send(PacketType::HI_AM_CLIENT);
		
		cout << "Awaiting ready signal" << endl;

        bool ready = false;

        // busy wait for a ready
        while (isConnected() && !ready) {
            for (NetMessageIterator& iter = connection->incomingMessageIterator(); iter.isValid(); ++iter){
                switch(iter.type()){
					case PacketType::TERMINATE:
						// the_app.terminate(); or something
						cout << "Network was terminated" << endl;
						return;
                    case PacketType::READY:
						// exit so the app can run
                        ready = true;
                        break;
                    default: break;
                }
            }
        }
    }

    // checks the network once and handles all available messages
    // wrap in a loop to repeatedly poll the network
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
                    // delete connection
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

    // Use this method to mark an entity to be updated on the network
    void Client::setEntityChanged(shared_ptr<Entity> e){
        // safety check
        changed_entities.insert(getEntityIDByName(e->name()));
    }

	// send an update on the network with a batch ID
	// the processed batch frame will need to return by the next deadline
	// or else the client will use a low qual render instead
    void Client::sendUpdate(){

        // new batch id
        current_batch_id++;

        // serialize 
		BinaryOutput* batch = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);

        // this currently loops through every entity
        // this is inefficient and should be improved such that we only iterate through a 
        // set of ids that were changed
        int id = 0;
        for (Array<shared_ptr<Entity>>::iterator it = entities.begin(); it != entities.end(); ++it){
            shared_ptr<Entity> ent = *it;

            float x,y,z,yaw,pitch,roll;
            ent->frame().getXYZYPRRadians(x,y,z,yaw,pitch,roll);

            batch->writeUInt32(id++);
			batch->writeFloat32(x);
			batch->writeFloat32(y);
			batch->writeFloat32(z);
			batch->writeFloat32(yaw);
			batch->writeFloat32(pitch);
			batch->writeFloat32(roll);
        }

        // net message send batch to router ip
        send(PacketType::UPDATE, *BinaryUtils::toBinaryOutput(current_batch_id), *batch);

        // clear recently used
        // changed_entities.erase();
    }
}
