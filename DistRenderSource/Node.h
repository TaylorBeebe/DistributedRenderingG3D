#pragma once
#include "RemoteRenderer.h"

using namespace RemoteRenderer;

// Abstract definitions for network nodes

namespace RemoteRenderer{
	namespace Node{

	    class NetworkNode{
	    	protected:
	    	public:
	    		NetworkNode(){}

	    		// app hooks
	    		void onUpdate() {};

	    		// socket hooks
	    		void onConnection() {};
	    		void onServerReady() {};
	    		void onData(G3D::BinaryInput& bitstream) {};
	    }

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

	    		const void send(G3D::BinaryOutput& bitstream) {
	    			socket->send(bitstream);
	    		}
	    }

	    // TODO: IMPLEMENT
	    class MultiConnectionNode : public NetworkNode {
	    	protected: 
	    	public:
	    		MultiConnectionNode() : NetworkNode() {}
	    }
	}
}