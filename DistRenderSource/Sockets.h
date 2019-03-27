#pragma once
#include "RemoteRenderer.h"

using namespace RemoteRenderer;

// Abstract definitions for network nodes

namespace RemoteRenderer{
	
    class SingleSocket : public WebServer::WebSocket {
		protected:
			Node::SingleConnectionNode* node; 
			SingleSocket(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) : WebSocket(server, connection, clientAddress) {}
		public:
			static shared_ptr<WebSocket> create(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) {
				return createShared<SingleSocket>(server, connection, clientAddress);
			}

			bool onConnect() override;

			void onReady() override;

			bool onData(Opcode opcode, char* data, size_t data_len) override;
	}

	class ServerSocket : public WebServer::WebSocket { 
		protected:
			Node::MultiConnectionNode* node; 

			unsigned int socket_id; 

			ServerSocket(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) : WebSocket(server, connection, clientAddress) {}
		public:
			static shared_ptr<WebSocket> create(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) {
				return createShared<ServerSocket>(server, connection, clientAddress);
			}

			bool onConnect() override;

			void onReady() override;

			bool onData(Opcode opcode, char* data, size_t data_len) override;
	}


}