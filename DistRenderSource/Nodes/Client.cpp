#include "Node.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

    Client::Client() : NetworkNode(constants::ROUTER_ADDR, NodeType::CLIENT) {}

    // @pre: incoming data packet
    // @post: saves the frame and renders it on the dealine (hopefully)
    void Client::onData(RenderPacket& packet){

        switch(packet.getPacketType()){
            case PacketType::FRAME:

                shared_ptr<G3D::Image> frame = G3D::Image::fromBinaryInput(packet.getBody(), new ImageFormat::RGB8())

                // cache in frame buffer

                break;
            case PacketType::READY:
                // if the client receives a ready from the router, this means that the router
                // has contact with all nodes and is ready to start 

                running = true;
                
                // start game tick

                break;
            case PacketType::END:
                // clean up
                break;
            default:
                debugPrintf("Client received incompatible packet type\n");
                break;   
        }
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

        RenderPacket packet (TRANSFORM, current_batch_id);
        packet.setBody(batch);

        // net message send batch to router ip
        send(packet);

        // clear recently used
        // changed_entities.erase();
    }


}
