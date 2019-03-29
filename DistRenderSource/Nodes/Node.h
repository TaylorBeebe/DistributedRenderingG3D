#pragma once
#include "RemoteRenderer.h"

using namespace RemoteRenderer;

// Abstract definitions for network nodes
namespace RemoteRenderer{
    namespace Node{

        // ABSTRACT NETWORK NODE CLASS
        // defines behavior of a node on the network with hooks for sockets and the GApp
        class NetworkNode{
            protected:

                NodeType type; 

                // Each entity in the scene will have a registered network ID
                // which will be synced across the network at setup so that at 
                // runtime transform data can be synced 
                map<unsigned int, G3D::Entity*> entityRegistry;

                uint net_nonce = 0;

            public:
                NetworkNode(NodeType t) : type(t) {}

                // @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
                // @post: creates network ID for entity and stores reference to it
                // @returns: network ID of entity 
                uint registerEntity(Entity* e){
                    entityRegistry[net_nonce] = e;
                    return net_nonce++;
                } 

                bool isTypeOf(NodeType t){ return t == type; }

                // app hooks
                void onUpdate() {};

                // socket hooks
                void onConnectionReady(uint socket_id) {};
                void onData(uint socket_id, RenderPacket* packet) {};
        }

        // SINGLE CONNECTION NODE CLASS
        // abstract class for a node that only handles a single connection
        // i.e. a client or remote render node
        class SingleConnectionNode : public NetworkNode {
            protected:
                shared_ptr<WebSocket> socket;
            public:
                SingleConnectionNode(NodeType type) : NetworkNode(type) {
                    // TODO: webserver needs a specification to be created
                    WebServer* server = new WebServer(specification);

                    // TODO: need to get the mg_connection some how and server_address
                    socket = RSocket::create(server, mg_connection, server_address);
                    socket->node = this;
                }

                const void send(RenderPacket* packet) {
                    socket->send(*(packet->toBinary()));
                }
        }
    }
}