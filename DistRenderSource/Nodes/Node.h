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
                const uint FRAMERATE = 30;

                bool isClient = false;
                bool isServer = false;
                bool isRemote = false;

                // Each entity in the scene will have a registered network ID
                // which will be synced across the network at setup so that at 
                // runtime transform data can be synced 
                map<unsigned int, G3D::Entity*> entityRegistry;

                uint net_nonce = 0;

            public:
                NetworkNode(){}

                // @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
                // @post: creates network ID for entity and stores reference to it
                // @returns: network ID of entity 
                uint registerEntity(Entity* e){
                    entityRegistry[net_nonce] = e;
                    return net_nonce++;
                } 

                // app hooks
                void onUpdate() {};

                // socket hooks
                void onConnection() {};
                void onServerReady() {};
                void onData(uint socket_id, RenderPacket* packet) {};
        }

        // SINGLE CONNECTION NODE CLASS
        // abstract class for a node that only handles a single connection
        // i.e. a client or remote render node
        class SingleConnectionNode : public NetworkNode {
            protected:
                shared_ptr<WebSocket> socket;
            public:
                SingleConnectionNode() : NetworkNode() {
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

        // TODO: Implement, probably change this to Server because only server inherits this
        class MultiConnectionNode : public NetworkNode {
            protected: 
                std::vector<shared_ptr<WebSocket>> sockets; 
            public:
                MultiConnectionNode() : NetworkNode() {}
        }
    }
}