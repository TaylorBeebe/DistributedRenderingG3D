#include <G3D/G3D.h>
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
 * REGISTRATION:
 *
 * Upon starting, a server will listen for connections. The server
 * will listen indefinitely until a Client connects, at which point
 * the server will make a deadline for remote nodes to connect, after
 * which it will begin when it has at least one remote node. At this
 * point the router will ignore any incoming messages that it has not
 * already registered as a remote or client node.
 *
 *
 * CONFIG:
 * 
 * Given valid connections the router will calculate the screen fragments 
 * for each remote node and send a CONFIG packet with that info to each 
 * node respectively. It is up to the remote nodes to respond with a 
 * CONFIG_RECEIPT packet asserting that the nodes successfully started their 
 * applications with the received screen data. The router will 
 * tally the responses, and when all are accounted for, the router
 * signals the client to start by broadcasting a READY packet to 
 * the network, also signalling the remote nodes.
 *
 *
 * RUNNING:
 *
 * On reception of an UPDATE packet, the router will reroute
 * the packet to all remote nodes. If the current frame build
 * is not complete, the rotuer will flush it and reset because that
 * frame missed the deadline on the client by now.
 *
 * On reception of a FRAGMENT packet, the router will add it 
 * to the build buffer for the current frame. If this makes the 
 * build buffer is full, the router will send the finished frame
 * to the client as a PNG.
 *
 * Coming soon, dynamic reallocation on node failure
 *
 *
 * TERMINATION:
 *
 * On reception of a TERMINATE packet from the client, the router
 * will broadcast the terminate packet and cut off all connections.
 *
 */

enum RouterState {
    OFFLINE,
    REGISTRATION,
    CONFIG,
    RUNNING,
    TERMINATED
};

typedef struct {
    bool configured;
    uint32 id;
    uint32 y;
    uint32 h;
    shared_ptr<NetConnection> connection;
} remote_connection_t;

RouterState router_state = OFFLINE;

shared_ptr<NetServer> server;
shared_ptr<NetConnection> client = nullptr;

uint32 current_batch;
uint32 pieces = 0;
// -- some pixel buffer

int configurations = 0;

// this registry will track remote connections, addressable with IP addresses
map<uint32, remote_connection_t*> remote_connection_registry;

// =========================================
//                  Setup
// =========================================

void addClient(shared_ptr<NetConnection> conn){

    cout << "Connected to client" << endl;

    // first come first serve
    if (client != NULL) return;

    client = conn; 
    // acknowledge
    conn->send(PacketType::ACK, BinaryUtils::empty(), BinaryUtils::empty(), 0);
}

void addRemote(shared_ptr<NetConnection> conn){

    uint32 id = conn->address().ip();

    if(remote_connection_registry.find(id) != remote_connection_registry.end()) return;

    remote_connection_t* cv = new remote_connection_t();
    cv->id = id;
    cv->configured = false;

    // defaults
    cv->y = 0;
    cv->h = 0;

	cv->connection = conn;
	remote_connection_registry[id] = cv;

    // acknowledge
	
    conn->send(PacketType::ACK, BinaryUtils::empty(), 0);

    cout << "Remote node with address " << id << " registered" << endl;
}

void removeRemote(NetAddress& addr){
    // TODO: dynamic resource reallocation
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

        cout << "Sending CONFIG packet to Remote Node " << cv->id << endl;
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

void broadcast(PacketType t, BinaryOutput& header, BinaryOutput& body, bool include_client) {

	cout << "Broadcasting message..." << endl;

	// optionally send to client
	if (include_client) client->send(t, body, header, 0);

	map<uint32, remote_connection_t*>::iterator iter;
	for (iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++) {
		iter->second->connection->send(t, body, header, 0);
	}
}

void terminateConnections() {
	cout << "Shutting down." << endl;
	broadcast(PacketType::TERMINATE, BinaryUtils::empty(), BinaryUtils::empty(), true);

	if (client != NULL) client->disconnect(false);

	map<uint32, remote_connection_t*>::iterator iter;
	for (iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++) {
		iter->second->connection->disconnect(false);
	}
}

void tallyConfigs(remote_connection_t* conn_vars) {
    if(!conn_vars->configured){
        conn_vars->configured = true;
        configurations++;
    }

    // if everyone is accounted for and running without error
    // broadcast a ready message to every node and await the client's start
    if(configurations == remote_connection_registry.size()){
        broadcast(PacketType::READY, BinaryUtils::empty(), BinaryUtils::empty(), true);

        cout << "----------------" << endl;
        cout << "NETWORK IS READY" << endl;
        cout << "----------------" << endl;
    }

    router_state = RUNNING;
}

void rerouteUpdate(BinaryInput& header, BinaryInput& body) {
   
    header.beginBits();
    current_batch = header.readUInt32();
    header.endBits();

	cout << "Rerouting update packet no. " << current_batch << endl;

    // reset batch variables
    flushPixelBuffer();
    pieces = 0;

    // route transform data to all remotes
    broadcast(PacketType::UPDATE, 
              BinaryUtils::toBinaryOutput(header), 
              BinaryUtils::toBinaryOutput(body), 
              false);
}

void handleFragment(BinaryInput& header, BinaryInput& body) {

    header.beginBits();
    uint32 batch_id = header.readUInt32();
    header.endBits();

    // old frag, toss out
    if (batch_id != current_batch) return;

    // attach fragment to buffer
    // stitch();

    // check if finished
    if (++pieces == remote_connection_registry.size()){
        cout << "Sending frame no. " << batch_id << " to client" << endl;

        // send a new frame packet to the client
        // BinaryOutput& data = BinaryUtils::empty();

        //Image frame;
        // ... 
        //frame.serialize(data, Image::PNG);

        //client->send(PacketType::FRAME, data, BinaryUtils::toBinaryOutput(batch_id), 0);
    }

} 

void listenAndRegister () {

    // default waiting period, won't matter because condition will short circuit
    RealTime tolerance = System::time(); 

    // listen until the client responds, and if the client responded wait until the tolerance is exceeded
    // then just use whatever nodes were registered. If there were no remote nodes, it will terminate in main
    while (client == NULL || System::time() < tolerance) {
        for(NetConnectionIterator niter = server->newConnectionIterator(); niter.isValid(); ++niter){
            shared_ptr<NetConnection> conn = niter.connection();
			cout << conn->address().ip() << endl;
            for (NetMessageIterator miter = conn->incomingMessageIterator(); miter.isValid(); ++miter) {
                try {
					cout << miter.type() << endl;
                    switch(miter.type()){
                        case PacketType::HI_AM_REMOTE:
                            addRemote(conn);
                            break;
                        case PacketType::HI_AM_CLIENT:
                            addClient(conn);
                            tolerance = System::time() + Constants::CONNECTION_WAIT;
                            break;
                        default:
                            cout << "Set up phase was not expecting packet of type " << miter.type() << endl; 
                    }
                } catch (...) {
                    cout << "An error occured" << endl;
                }
            } // end message queue iterate
        } // end connections iterate
    } // end while
}

// This receive method will check for available messages forever unless a connection is compromised,
// it will check every connection in the remote connection registry and the client connection
// then call whatever code is specified with that message type
//
// This will only work with packets that have a body AND a header
void receive(){

    map<uint32, remote_connection_t*>::iterator remotes;

    while(router_state != TERMINATED){

        // TODO: make sure client is still connected
        if (false) {}

        // listen to client
        for(NetMessageIterator iter = client->incomingMessageIterator(); iter.isValid(); ++iter){
            try {
                switch(iter.type()){
                    case PacketType::UPDATE: // reroute update from clients
                        rerouteUpdate(iter.headerBinaryInput(), iter.binaryInput());
                        break;
                    case PacketType::TERMINATE: // the client wants to stop
                        router_state = TERMINATED;
                        break;
                    default:
                        cout << "Router received unexpected message " << iter.type() << " from client" << endl;
						break;
                }
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
                    switch(iter.type()){
                        case PacketType::FRAGMENT: // a frame fragment
                            cout << "remote " << conn_vars->id << "answered, total: " << (pieces + 1) << "/" << remote_connection_registry.size() << endl;
                            handleFragment(iter.headerBinaryInput(), iter.binaryInput());
                            break;
                        case PacketType::CONFIG_RECEIPT: // a receipt of configs
							tallyConfigs(conn_vars);
                            break;
                        default: // router received unkown message
                            cout << "Router received unexpected message " << iter.type() << " from remote node" << endl;
                            break;

                    } // end switch
                }catch(...){
                    // handle error
                }
            } // end remote message loop
        } // end remote connection loop
    } // end main loop
}

int main(){

	initG3D();

    cout << "Router started up" << endl;
    cout << "Initializing server..." << endl;

    server = NetServer::create(Constants::ROUTER_ADDR, 32, 1);

	cout << "Server initialized" << endl;
    cout << "Waiting for connections..." << endl;

    // registration phase
    router_state = REGISTRATION; 
    listenAndRegister();

    // config phase
    router_state = RouterState::CONFIG;

    if(remote_connection_registry.size() == 0) cout << "No remote nodes were registered." << endl;   
    else if (client == NULL) cout << "Client was NULL" << endl;
    else {
        // calculate screen data
        configureScreenSplit();
        // poll network for updates ad infintum
        receive();
    }

    terminateConnections();

    cout << "Goodbye." << endl; 

    return 0;
}