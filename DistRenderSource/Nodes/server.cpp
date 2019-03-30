#include "Client.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

    Server::Server() : NetworkNode(SERVER) {
        // TODO: webserver needs a specification to be created
        webserver = new WebServer(specification);
    }

    // @pre: network address of node
    // @post: creates a new RSocket, ids it, and adds it to the socket list
    void Server::addSocket(G3D::NetAddress* address, bool is_client_connection){

        // TODO: need to get the mg_connection some how and server_address
        shared_ptr<G3D::WebSocket> sock = RSocket::create(server, mg_connection, address);
        sock.setNode(&this);
        
        if(is_client_connection && client_socket == NULL){
            sock.setSocketId(CLIENT_ID);
            client_socket = sock;
        }else{
            sock.setSocketId(100);
            remotes.push_back(sock);
        }

    }

    // @pre: incoming data packet
    // @post: reroutes packet depending on what type of node sent it
    void Server::onData(uint socket_id, RenderPacket* packet){
        if(socket_id == CLIENT_ID) onClientData(packet);
        else onRemoteData(packet);
    }

    void Server::onClientData(RenderPacket* packet){
        switch(packet->getPacketType()){
            case TRANSFORM:
            
                // reroute transform data
                for (std::list<transform_t*>::iterator it = remotes.begin(); it != remotes.end(); ++it) 
                    it->sendPacket(packet);

                break;
            case FRAME:
                // wrong thing from the client
                break;
        }
    }   

    void Server::onRemoteData(RenderPacket* packet){
        switch(packet->getPacketType()){
            case TRANSFORM:
                // wrong thing from the client
                break;
            case FRAME:

                // TODO: combine frame data in buffer and if all there send to client

                // send a new frame packet to the client
                FramePacket* frame_data = new FramePacket(packet->getBatchId());
                client_socket->sendPacket(packet);

                break;
        }
    }

}
