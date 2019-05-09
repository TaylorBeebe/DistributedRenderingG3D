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
						cout << "Network is ready" << endl;
                        ready = true;
                        break;
                    default: break;
                }
            }
        }
    }

    // checks the network once and handles all available messages
    // wrap in a loop to repeatedly poll the network
    bool Client::checkNetwork(){

        NetMessageIterator& iter = connection->incomingMessageIterator();

        if(!iter.isValid()) return false;

        try{

            BinaryInput& header = iter.headerBinaryInput();
            uint32 batch_id = header.readUInt32();

			shared_ptr<Image> frame;

            switch(iter.type()){
                case PacketType::FRAME:

					frame = Image::fromBinaryInput(iter.binaryInput(), ImageFormat::RGB8());
                    // convert to texture and toggle flag
                    cout << "Received frame!" << endl;
                    return true;
                case PacketType::TERMINATE:
                    // clean up
                    break;
                default: // Client does not need this datatype
                    cout << "Client received incompatible packet type " << iter.type() << endl;
                    break;   
            } // end switch
        } catch(...) {
            // handle error
        }

        ++iter;
    }

	// send an update on the network with a batch ID
	// the processed batch frame will need to return by the next deadline
	// or else the client will use a low qual render instead
    void Client::sendUpdate(){

        // serialize 
		BinaryOutput* batch = BinaryUtils::create();

        for (int i = 0; i < entities.size(); i++){
            shared_ptr<Entity> ent = entities[i];

            if(ent->lastChangeTime() < last_update) continue;

            float x,y,z,yaw,pitch,roll;
            ent->frame().getXYZYPRRadians(x,y,z,yaw,pitch,roll);

            batch->writeUInt32(i);
			batch->writeFloat32(x);
			batch->writeFloat32(y);
			batch->writeFloat32(z);
			batch->writeFloat32(yaw);
			batch->writeFloat32(pitch);
			batch->writeFloat32(roll);
        }

        // net message send batch to router ip
        if(batch->length() > 0){
            send(PacketType::UPDATE, *BinaryUtils::toBinaryOutput(current_batch_id++), *batch);
            last_update = System::time();
        }

    }
}
