#pragma once
#include "Node.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

    class Server : public Node::NetworkNode {
        private: 
            std::list<shared_ptr<WebSocket>> remotes;
            shared_ptr<WebSocket> client_socket;
            G3D::WebServer* webserver;

            const uint CLIENT_ID = 0;
            
            virtual void onClientData(RenderPacket* packet);
            virtual void onRemoteData(RenderPacket* packet);
            
        public:
            Server();

            virtual void addSocket(G3D::NetAddress* address, bool is_client_connection);

            void onData(uint socket_id, RenderPacket* packet) override;
    }
}


        