#include "Router.h"

using namespace std;
using namespace DistributedRenderer;
using namespace G3D;

namespace DistributedRenderer{
namespace Router{

// =========================================
//                  Setup
// =========================================

    void Router::addClient(shared_ptr<NetConnection> conn){

        cout << "Connected to client" << endl;

        // first come first serve
        if (client != NULL) return;

        client = conn; 
    }

    void Router::addRemote(shared_ptr<NetConnection> conn){

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

        cout << "Remote node with address " << id << " registered" << endl;
    }

    void Router::removeRemote(NetAddress& addr){
        // TODO: dynamic rebalancing
    }


// =========================================
//              Frame Managing
// =========================================

    void Router::flushPixelBuffer() {}

    void Router::stitch(remote_connection_t* conn_vars, Image& fragment) {}



// =========================================
//              Packet Handling
// =========================================

    void Router::rerouteUpdate(BinaryInput& header, BinaryInput& body) {
       
        header.beginBits();
        current_batch = header.readUInt32();
        header.endBits();

        cout << "Rerouting update packet no. " << current_batch << endl;

        // reset batch variables
        flushPixelBuffer();
        pieces = 0;

        // route transform data to all remotes
        broadcast(PacketType::UPDATE, 
                  BinaryUtils::toBinaryOutput(&header), 
                  BinaryUtils::toBinaryOutput(&body), 
                  false);
    }

    void Router::handleFragment(remote_connection_t* conn_vars, BinaryInput& header, BinaryInput& body) {

        header.beginBits();
        uint32 batch_id = header.readUInt32();
        header.endBits();

        // old frag, toss out
        if (batch_id != current_batch) return;

        // attach fragment to buffer
        // stitch();

        // check if finished
        if (++pieces == numRemotes()){
            cout << "Sending frame no. " << batch_id << " to client" << endl;

            // send a new frame packet to the client
            // BinaryOutput& data = BinaryUtils::empty();

            //Image frame;
            // ... 
            //frame.serialize(data, Image::PNG);

            //send(PacketType::FRAME, data, BinaryUtils::toBinaryOutput(batch_id), 0);
        } 
    }

// =========================================
//                Networking
// =========================================

    void Router::broadcast(PacketType t, BinaryOutput* header, BinaryOutput* body, bool include_client) {

    	cout << "Broadcasting message..." << endl;

    	// optionally send to client
    	if (include_client) send(t, client, header, body);

    	map<uint32, remote_connection_t*>::iterator iter;
    	for (iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++) {
            send(t, iter->second->connection, header, body);
    	}
    }

    void Router::broadcast(PacketType t, bool include_client) {
    	broadcast(t, BinaryUtils::empty(), BinaryUtils::empty(), include_client);
    }

    void Router::send(PacketType t, shared_ptr<NetConnection> conn, BinaryOutput* header, BinaryOutput* body){
        // do any send preparations here
        conn->send(t, *header, *body, 0);
    }

    void Router::send(PacketType t, shared_ptr<NetConnection> conn){
        send(t, conn, BinaryUtils::empty(), BinaryUtils::empty());
    }

    void Router::registration() {
        setState(REGISTRATION);

        // default waiting period, won't matter because condition will short circuit
        RealTime tolerance = System::time(); 

        // cache connected machines 
        list<shared_ptr<NetConnection>> connections;

        // listen until the client responds, and if the client responded wait until the tolerance is exceeded
        // then just use whatever nodes were registered. If there were no remote nodes, it will terminate in main
        while (client == NULL || System::time() < tolerance) {
            // If we directly check the message iterator after we get the connection, it will not always
            // give us the messages even though it has them because it hasn't initialized its NetServerSideConnection
            // so we just cache the connection and always recheck it afterwards
            for (NetConnectionIterator niter = server->newConnectionIterator(); niter.isValid(); ++niter) {
                shared_ptr<NetConnection> conn = niter.connection();
                connections.push_back(conn);
            }

            for (list<shared_ptr<NetConnection>>::iterator it = connections.begin(); it != connections.end(); ++it) {
                shared_ptr<NetConnection> conn = *it;
                for (NetMessageIterator miter = conn->incomingMessageIterator(); miter.isValid(); ++miter) {
                    try {
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

    void Router::configuration() {
        setState(CONFIGURATION);

        // TODO: if the screen height is not perfectly divisble by the number of nodes, give the remaining pixels
        // to one of the nodes
        uint32 frag_height = Constants::SCREEN_HEIGHT / numRemotes(); 
        uint32 curr_y = 0;

        map<uint32, remote_connection_t*>::iterator iter;
        for(iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++){ 

            remote_connection_t* cv = iter->second;

            // send the config data
            BinaryOutput* config = new BinaryOutput( "<memory>", G3DEndian::G3D_BIG_ENDIAN );

            config->writeUInt32(curr_y);
            config->writeUInt32(frag_height);

            cout << "Sending CONFIG packet to Remote Node " << cv->id << " offset_y: " << curr_y << ", height: " << frag_height << endl;
            send(PacketType::CONFIG, cv->connection, BinaryUtils::empty(), config);

            // store internal record
            cv->y = curr_y;
            cv->h = frag_height;

            curr_y += frag_height;
        }

        int configurations = 0;
        map<uint32, remote_connection_t*>::iterator remotes;

        while(router_state != TERMINATED){
            for(remotes = remote_connection_registry.begin(); remotes != remote_connection_registry.end(); remotes++){
                remote_connection_t* conn_vars = remotes->second;
                shared_ptr<NetConnection> conn = conn_vars->connection;

                for(NetMessageIterator iter = conn->incomingMessageIterator(); iter.isValid(); ++iter){
                    try {  
                        switch(iter.type()){
                            case PacketType::CONFIG_RECEIPT: // a receipt of configs

                                if(!conn_vars->configured){
                                    conn_vars->configured = true;
                                    configurations++;
                                }

                                // if every node is accounted for and running without error
                                // broadcast a ready message and await the client's update
                                if(configurations == numRemotes()){
                                    broadcast(PacketType::READY, true);

                                    cout << "----------------" << endl;
                                    cout << "NETWORK IS READY" << endl;
                                    cout << "----------------" << endl;
                                }

                                return;
                            case PacketType::TERMINATE:
                                // handle failure
                                break;
                            default: // router received unkown message
                                cout << "Router received unexpected message of type " << iter.type() << " from remote node" << endl;
                                break;
                        }
                    }catch(...){
                        // handle error
                    }
                } // end remote message loop
            } // end remote connection loop
        } // end main loop
    }


    bool Router::setup() {

        server = NetServer::create(Constants::ROUTER_ADDR, 32, 1);

        cout << "Waiting for connections to register..." << endl;
        registration();

        if(numRemotes() == 0){
            cout << "No remote nodes were registered." << endl;   
            return false;
        } else if (client == NULL) {
            cout << "Client connection could not be initalized" << endl;
            return false;
        }

        cout << "Connections established. Configuring remote nodes..." << endl;
        configuration();

        return true;
    }

    // This receive method will check for available messages forever unless a connection is compromised,
    // it will check every connection in the remote connection registry and the client connection
    // then call whatever code is specified with that message type
    void Router::poll(){
        setState(LISTENING);

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
                            setState(TERMINATED);
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
                                cout << "remote " << conn_vars->id << "answered, total: " << (pieces + 1) << "/" << numRemotes() << endl;
                                handleFragment(conn_vars, iter.headerBinaryInput(), iter.binaryInput());
                                break;
                            case PacketType::TERMINATE:
                                // handle failure
                                break;
                            default: // router received unkown message
                                cout << "Router received unexpected message of type" << iter.type() << " from remote node" << endl;
                                break;
                        }
                    }catch(...){
                        // handle error
                    }
                } // end remote message loop
            } // end remote connection loop
        } // end main loop
    }

    void Router::terminate() {
        cout << "Shutting down." << endl;
        broadcast(PacketType::TERMINATE, true);

        if (client != NULL) client->disconnect(false);

        map<uint32, remote_connection_t*>::iterator iter;
        for (iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++) {
            iter->second->connection->disconnect(false);
        }

        //delete *server;
        //delete *client;
    }   
}
}