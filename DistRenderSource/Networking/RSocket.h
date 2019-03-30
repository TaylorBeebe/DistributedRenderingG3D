#pragma once
#include "RemoteRenderer.h"

using namespace RemoteRenderer;

// Abstract definitions for network nodes

namespace RemoteRenderer{
    
    class RSocket : public WebServer::WebSocket {
        protected:
            Node::NetworkNode* node; 
            uint socket_id = 0;
            RSocket(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) : WebSocket(server, connection, clientAddress) {}
        public:
            static shared_ptr<WebSocket> create(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) {
                return createShared<RSocket>(server, connection, clientAddress);
            }

            void setSocketId(uint id){ socket_id = id; }
            void setNode(Node::NetworkNode* n) {node = n;}
            virtual void sendPacket(* packet);

            bool onConnect() override;
            void onReady() override;
            bool onData(Opcode opcode, char* data, size_t data_len) override;

    }


}