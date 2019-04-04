#include "Router.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{


    // set up all net connections 
    // after all connections are set up, we send an omni handshake with screen info (if they're remote)
    // they will not respond immediately but once their application is setup and they have received the 
    // screen data they will respond with their own handshake the router will tally up the handshakes 
    // and when all handshakes are accounted for it will broadcast a ready message
    Router::Router() {
        net_server = NetServer::create(Router, 32, G3D::UNLIMITED_BANDWIDTH, G3D::UNLIMITED_BANDWIDTH);

        // set up connections

        // calculate screen data 

        // handshake screen data to nodes
        
        // poll network for updates
        poll();
    }

    // digest available packets
    void Router::poll(){

        G3D::NetConnectionIterator iter = net_server->newConnectionIterator();

        while(running){
            for(; iter.isValid(); iter++){
                shared_ptr<G3D::NetConnection> conn = iter.connection();
                for(G3D::NetMessageIterator iter2 = conn->incomingMessageIterator(); iter2.isValid(); iter2++){
                    // make a render packet
                    RenderPacket packet ((PacketType) iter2.type(), iter2.headerBinaryInput(), iter2.binaryInput());
                    // call network node packet handler
                    onData(packet, *conn);
                }
            }
        }

    }

    void Router::addConnectionClient(G3D::NetAddress& address){

    }

    // @pre: network address of node
    // @post: creates a new RSocket, ids it, and adds it to the socket list
    void Router::addConnectionRemote(G3D::NetAddress& address){
        

    }

    // @pre: incoming data packet and socket it came from
    // @post: reroutes packet depending on what type of node sent it
    void Router::onData(RenderPacket& packet, G3D::NetConnection& connection){
         switch(packet.getPacketType()){
            case PacketType::TRANSFORM:

                // reset
                current_batch = packet.getBatchId();
                flushPixelBuffer();
                pieces = 0;
            
                // reroute transform data to all remotes
                // for (std::list<transform_t*>::iterator it = remotes.begin(); it != remotes.end(); ++it) 
                    // it->sendPacket(packet);

                break;
            case PacketType::FRAGMENT:

                // old frag, toss out
                if (packet.getBatchId() != current_batch) break;

                // attach fragment to buffer

                // check if finished
                if (++pieces == num_nodes){
                    // send a new frame packet to the client
                    RenderPacket packet (PacketType::FRAME, current_batch);
                    G3D::BinaryOutput data ();
                    data.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

                    G3D::Image frame; // get from buffer

                    frame.serialize(data, Image::JPEG);

                    packet.setBody(data);
                    // send to client
                }

                break;
            case PacketType::HANDSHAKE:
                // do accounting
                // if everyone is accounted for and running without error
                running = true;
                // broadcast a ready message to every node and await the client's start
                G3D::BinaryOutput bo ();
                bo.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);
                net_server->omniConnection()->send(PacketType::READY, bo, 0);
            default:
                // strange packet
                break;
        }
    }

}
