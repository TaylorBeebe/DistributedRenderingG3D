#include "DistributedRenderer.h"

using namespace std;
using namespace DistributedRenderer;
using namespace G3D;

/* =========================================
 * =========================================
 *            Distributed Router
 * =========================================
 * =========================================
 * 
 * A Router built on G3D NetConnections to service
 * a remote rendering distributed network
 *
 * -----
 *
 * PROTCOL:
 * 
 * Upon starting, the server will establish connections with all
 * specified remote nodes and the client application. Given valid
 * connections, the router will calculate the screen fragments
 * for each remote node and send a CONFIG packet with that info
 * to each node respectively
 *
 * It is up to the remote nodes to respond with a CONFIG_RECEIPT
 * packet asserting that the nodes successfully started their 
 * applications and received the screen data. The router will 
 * tally the responses, and when all are accounted for, the router
 * signals the client to start by broadcasting a READY packet to 
 * the network, also signalling the remote nodes
 *
 * On reception of a TRANSFORM packet, the router will reroute
 * the packet to all remote nodes. If the current frame build
 * is not complete, we can flush it and reset because that
 * frame missed the deadline.
 *
 * On reception of a FRAGMENT packet, the router will add it 
 * to the build buffer for the current frame. If this makes the 
 * build buffer is full, the router will send the finished frame
 * to the client as a PNG.
 */

typedef struct {
    bool configured;
    uint32 id;
    uint32 y;
    uint32 h;
    shared_ptr<NetConnection> connection;
} remote_connection_t;

// APP VARIABLES
// ...

// PIXELS
uint32 current_batch;
uint32 pieces = 0;
// -- some pixel buffer

// NETWORKING
uint32 nonce = 0; // for basic, fast remote identifiers
uint32 configurations = 0; // internal tally of configured remotes

// this registry will track remote connections, addressable with IDs
map<uint32, remote_connection_t*> remote_connection_registry;
// the client connection
shared_ptr<NetConnection> client = nullptr;

// =========================================
//                  Setup
// =========================================

void addRemote(NetAddress& addr){

    shared_ptr<NetConnection> conn = nullptr;
    if(!connect(addr, conn)) return;

    uint32 id = nonce++;

    remote_connection_t* cv = new remote_connection_t();
    cv->id = id;
    cv->configured = false;

    // default
    cv->y = 0;
    cv->h = 0;

	cv->connection = conn;
	remote_connection_registry[id] = cv;
}

void removeRemote(NetAddress& addr){
    // more complicated
}

void configureScreenSplit(){
    configurations = 0;

    // TODO: if the screen height is not perfectly divisble by the number of nodes, give the remaining pixels
    // to one of the nodes
    uint32 frag_height = Constants::SCREEN_HEIGHT / remote_connection_registry.size(); 
    uint32 curr_y = 0;

    map<uint32, remote_connection_t*>::iterator iter;
    for(iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++){ 

		remote_connection_t* cv = iter->second;

        // send the config data
		uint32 conf_attrs[] = { curr_y, frag_height };
        BinaryOutput& config = BinaryUtils::toBinaryOutput( conf_attrs );
        cv->connection->send(PacketType::CONFIG, BinaryUtils::empty(), config, 0);

        // store internal record
        cv->y = curr_y;
        cv->h = frag_height;

        curr_y += frag_height;
    }
}

// =========================================
//              Frame Buffering
// =========================================

void flushPixelBuffer() {}

void stitch(Image& fragment, uint32 x, uint32 y) {}

// =========================================
//                Networking
// =========================================

void broadcast(PacketType t, BinaryOutput& header, BinaryOutput& body, bool include_client){
    // optionally send to client
    if(include_client) client->send(t, body, header, 0);

    map<uint32, remote_connection_t*>::iterator iter;
    for(iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++){ 
        iter->second->connection->send(t, body, header, 0);
    }
}

void terminateConnection(){
    broadcast(PacketType::TERMINATE, BinaryUtils::empty(), BinaryUtils::empty(), true);

    client->disconnect(false);
    map<uint32, remote_connection_t*>::iterator iter;
    for(iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++){ 
        iter->second->connection->disconnect(false);
    }
}

// This receive method will check for available messages forever unless a connection is compromised,
// it will check every connection in the remote connection registry and the client connection
// then call whatever code is specified with that message type
//
// This will only work with packets that have a body AND a header
void receive(){

    map<uint32, remote_connection_t*>::iterator remotes;

	uint32 batch_id;

    while(true){

        // TODO: make sure client is still connected
        if (false) {}

        // listen to client
        for(NetMessageIterator iter = client->incomingMessageIterator(); iter.isValid(); ++iter){
            try {
				BinaryInput& header = iter.headerBinaryInput();
                header.beginBits();

                batch_id = header.readUInt32();

                switch(iter.type()){
                    case PacketType::UPDATE: // frequent update from clients

                        // reset batch variables
                        current_batch = batch_id;
                        flushPixelBuffer();
                        pieces = 0;
                    
                        // route transform data to all remotes
                        broadcast(PacketType::UPDATE, 
                                  BinaryUtils::toBinaryOutput(header), 
                                  BinaryUtils::toBinaryOutput(iter.binaryInput()), 
                                  false);

                        break;
                    case PacketType::TERMINATE: // the client wants to stop
                        terminateConnection();
                        break;
                    default: // don't need this
						break;
                }

                header.endBits();

            }catch(...){
                // handle client error
            }
        } // client message loop

        // listen to remote connections
        for(remotes = remote_connection_registry.begin(); remotes != remote_connection_registry.end(); remotes++){
            remote_connection_t* conn_vars = remotes->second;
            shared_ptr<NetConnection> conn = conn_vars->connection;

            // TODO: check if node is still connected
            if (false) {}

            for(NetMessageIterator iter = conn->incomingMessageIterator(); iter.isValid(); ++iter){
                try {  

                    BinaryInput& header = iter.headerBinaryInput();
                    header.beginBits();
                    batch_id = header.readUInt32();

                    switch(iter.type()){
                        case PacketType::FRAGMENT:

                            // old frag, toss out
                            if (batch_id != current_batch) break;

                            // attach fragment to buffer

                            // check if finished
                            if (++pieces == remote_connection_registry.size()){
                                // send a new frame packet to the client
                                BinaryOutput& data = BinaryUtils::empty();

                                //Image frame;
                                // ... 
                                //frame.serialize(data, Image::PNG);

                                //client->send(PacketType::FRAME, data, BinaryUtils::toBinaryOutput(batch_id), 0);
                            }

                            break;

                        case PacketType::CONFIG_RECEIPT:

                            // do accounting
                            if(!conn_vars->configured){
                                conn_vars->configured = true;
                                configurations++;
                            }

                            // if everyone is accounted for and running without error
                            // broadcast a ready message to every node and await the client's start
                            if(configurations == remote_connection_registry.size()){
                                broadcast(PacketType::READY, BinaryUtils::empty(), BinaryUtils::empty(), true);
                            }

                            break;

                        default: // router received unkown message
                            break;

                    } // end switch

                    header.endBits();

                }catch(...){
                    // handle error
                }
            } // end remote message loop
        } // end remote connection loop
    } // end main loop
}

int main(){

    // set up connections (in the future make this dynamic with a reference list or something)
    addRemote(Constants::N1_ADDR);
    addRemote(Constants::N2_ADDR);
    addRemote(Constants::N3_ADDR);

    // Attempt connection to client
    // if the connection to the client is compromised or there are no remote node connections
    // broadcast a terminate message
    if(!connect(Constants::CLIENT_ADDR, client) || remote_connection_registry.size() == 0){
        terminateConnection();
        return 0;
    }

    // calculate screen data
    configureScreenSplit();
    // poll network for updates ad infintum
    receive(); 

    return 0;
}