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
        sock->node = this;
        
        if(is_client_connection && client_socket == NULL){
            sock->socket_id = -1;
            client_socket = sock;
        }else{
            sock->socket_id = id_nonce++;
            sockets.add(sock);
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
                G3D::BinaryOutput data = *(packet->toBinary());
                for(int i = 0; i < sockets.size(); i++) 
                    sockets[i]->send(data); 

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

                // send a frame packet to the client
                FramePacket* frame_data = new FramePacket(packet->getBatchId());
                client_socket->send(*(frame_data->toBinary()));

                break;
        }
    }

}
