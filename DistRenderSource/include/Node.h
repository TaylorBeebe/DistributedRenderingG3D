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
	    	public:
	    		NetworkNode(){}

	    		// app hooks
	    		void onUpdate() {};

	    		// socket hooks
	    		void onConnection() {};
	    		void onServerReady() {};
	    		void onData(RenderPacket* packet) {};
	    }

	    // SINGLE CONNECTION NODE CLASS
	    // abstract class for a node that only handles a single connection
	    // i.e. a client or remote render node
	    class SingleConnectionNode : public NetworkNode {
	    	protected:
	    		shared_ptr<WebSocket> socket;
	    	public:
	    	 	SingleConnectionNode() : NetworkNode() {
	    	 		// webserver needs a specification to be created
				    WebServer* server = new WebServer(specification);

				    // need to get the mg_connection some how and server_address
				    socket = SingleSocket::create(server, mg_connection, server_address);
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