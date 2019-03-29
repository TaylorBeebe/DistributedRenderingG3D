#pragma once
#include "Node.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

    class Server : public Node::NetworkNode {
        private: 

            std::vector<shared_ptr<WebSocket>> sockets;
            G3D::WebServer* webserver;

            uint id_nonce = 0;

            const uint CLIENT_ID = -1;
            shared_ptr<WebSocket> client_socket;

            virtual void onClientData(RenderPacket* packet);
            virtual void onRemoteData(RenderPacket* packet);
            
        public:
            Server();

            virtual void addSocket(G3D::NetAddress* address, bool is_client_connection);

            void onData(uint socket_id, RenderPacket* packet) override;
    }
}


        