#include "DistributedRenderer.h"

using namespace DistributedRenderer;

namespace DistributedRenderer{

    Remote::Remote(RApp* app, bool headless_mode) : NetworkNode(NodeType::REMOTE, app, headless_mode) {}

    void Remote::onConnect() {

		cout << "Connected to router" << endl;

        // send router intoduction
        send(PacketType::HI_AM_REMOTE);

		cout << "Awaiting configuration" << endl;

        bool ready = false;

        // wait for a config
        // then wait for a ready
        while (isConnected() && !ready) {
            for (NetMessageIterator& iter = connection->incomingMessageIterator(); iter.isValid(); ++iter){
                switch(iter.type()){
                    case PacketType::CONFIG:
                        cout << "Received CONFIG, configuring..." << endl;
                        setClip(&iter.binaryInput());
                        send(PacketType::CONFIG_RECEIPT);
                        break;
                    case PacketType::READY:
						cout << "Network is ready" << endl;
                        ready = true;
                        break;
					case PacketType::TERMINATE:
						cout << "Network was terminated" << endl;
						// the_app.terminate(); or something
						return;
                    default: 
                        cout << "Received unexpected packet" << endl;
                        break;
                }
            }
        }
    }

    void Remote::setClip(uint32 y, uint32 height){
        bounds = Rect2D::xywh(0, y, Constants::SCREEN_WIDTH, height);
    }

	void Remote::setClip(BinaryInput* bi) {
		uint32 y = bi->readUInt32();
		uint32 h = bi->readUInt32();

        cout << "Config delivered height: " << h << ", y: " << y << endl;

		setClip(y, h);
	}

    void Remote::receive() {

        NetMessageIterator& iter = connection->incomingMessageIterator();
        if(!iter.isValid()) return;
        
        try{
            // read the header
            BinaryInput& header = iter.headerBinaryInput();
            uint32 batch_id = header.readUInt32();

            switch(iter.type()){
                case PacketType::UPDATE: // update data
                    sync(&iter.binaryInput());
                    the_app->oneFrameAdHoc();
                    sendFrame(batch_id);
                    break;

                case PacketType::TERMINATE: // this is the end of all messages
                    cout << "Terminate received" << endl;
                    // clean up app
					// delete connection
                    break;

                default: // Remote Node does not need this datatype
                    debugPrintf("Remote Node received incompatible packet type\n");

            } // end switch

        } catch(...) { // something went wrong decoding the message
            // handle error or do nothing
        }

        // pop the message off of the queue
        ++iter;
        
    }

    // @pre: transform packet with list of transforms of entities to update
    // @post: updates frame of corresponding entity with new position data
    void Remote::sync(BinaryInput* update){

        cout << "Syncing update..." << endl;

        while(update->hasMore()){
            uint32 id = update->readUInt32();
            float x = update->readFloat32();
            float y = update->readFloat32();
            float z = update->readFloat32();
            float yaw = update->readFloat32();
            float pitch = update->readFloat32();
            float roll = update->readFloat32();

            CoordinateFrame nextframe = CoordinateFrame::fromXYZYPRRadians(x,y,z,yaw,pitch,roll);
            getEntityByID(id)->setFrame(nextframe, true);

        }
    }

    // @pre: the current batch id
    // @post: renders a new frame and sends it in a frame packet back to the router
    void Remote::sendFrame(uint32 batch_id){

        cout << "Sending fragment of frame " << batch_id << " to router" << endl;

        BinaryOutput* bo = BinaryUtils::create();
        BinaryOutput* header = BinaryUtils::toBinaryOutput(batch_id);

        shared_ptr<Framebuffer> buffer = the_app->finalFrameBuffer();
        Draw::rect2D(buffer->texture(0)->rect2DBounds(), the_app->renderDevice, Color3::white(), buffer->texture(0));

        shared_ptr<Image> frame = buffer->texture(0)->toImage(ImageFormat::RGB8());
        frame->serialize(*bo, Image::PNG);

        // send(PacketType::FRAGMENT, header, bo);

        delete bo; 
        delete header;
    }
}
