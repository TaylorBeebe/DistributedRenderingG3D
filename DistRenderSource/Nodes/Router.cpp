#include "Router.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{


    // set up all net connections 
    // after all connections are set up, we send an omni handshake with screen info (if they're remote)
    // they will not respond immediately but once their application is setup and they have received the 
    // screen data they will respond with their own handshake the router will tally up the handshakes 
    // and when all handshakes are accounted for it will broadcast a ready message
    Router::Router() : running(false),  pieces(0), nonce(0), handshakes(0){

        // init registry
        remote_connection_registry();

        // set up connections (in the future make this dynamic with a reference list or something)
        addClient(Constants::CLIENT_ADDR);
        addRemote(Constants::N1_ADDR);
        addRemote(Constants::N2_ADDR);
        addRemote(Constants::N3_ADDR);

        // calculate screen data
        // configureScreenData();
        
        // poll network for updates
        receive();
    }

    // This receive method will check for available messages as long as the running flag is set true
    // it will check every connection in the remote connection registry and the client connection
    // then call whatever code is specified with the message type
    void Router::receive(){

        map<uint, remote_connection_t*>::iterator remotes;

        NetMessageIterator iter;
        BinaryInput& header;
        uint batch_id;

        while(running){

            // listen to client
            for(iter = client->incomingMessageIterator(); iter.isValid(); iter++){
                try {
                    header = iter.headerBinaryInput();
                    header.beginBits();

                    batch_id = header.readUInt32();

                    switch(iter.type()){
                        case PacketType::TRANSFORM:

                            // reset batch variables
                            current_batch = batch_id;
                            flushPixelBuffer();
                            pieces = 0;
                        
                            // reroute transform data to all remotes
                            broadcast(PacketType::TRANSFORM, toBinaryOutput(header), toBinaryOutput(iter.binaryInput()), false);

                            break;
                        default: // don't need this
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

                for(iter = conn->incomingMessageIterator(); iter.isValid(); iter++){
                    try {  

                        header = iter.headerBinaryInput();
                        header.beginBits();
                        batch_id = header.readUInt32();

                        switch(iter.type()){
                            case PacketType::TRANSFORM:

                                // reset batch variables
                                current_batch = batch_id;
                                flushPixelBuffer();
                                pieces = 0;
                            
                                // reroute transform data to all remotes
                                broadcast(PacketType::TRANSFORM, toBinaryOutput(header), toBinaryOutput(iter.binaryInput()), false);

                                break;

                            case PacketType::FRAGMENT:

                                // old frag, toss out
                                if (batch_id != current_batch) break;

                                // attach fragment to buffer

                                // check if finished
                                if (++pieces == remote_connection_registry.size()){
                                    // send a new frame packet to the client
                                    G3D::BinaryOutput data ();
                                    data.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

                                    G3D::Image frame; // get from buffer
                                    frame.serialize(data, Image::JPEG);

                                    client->send(PacketType::FRAME, data, toBinaryOutput(batch_id), 0);
                                }

                                break;

                            case PacketType::HANDSHAKE:

                                // do accounting
                                if(!conn_vars->handshake){
                                    conn_vars->handshake = true;
                                    handshakes++;
                                }

                                // if everyone is accounted for and running without error
                                // broadcast a ready message to every node and await the client's start
                                if(handshakes == remote_connection_registry.size()){
                                    running = true;
                                    broadcast(PacketType::READY, toBinaryOutput(0), toBinaryOutput(0), true);
                                }

                                break;

                            default: // router received unkown message
                                break;

                        } // end switch

                        header.endBits();

                    }catch(...){
                        // handle error
                    }
                } // remote message loop
            } // remote connection loop
        } // main loop
    }

    void Router::addClient(G3D::NetAddress& address){
        client = NetConnection::connectToServer(address, 1, UNLIMITED_BANDWIDTH, UNLIMITED_BANDWIDTH);
    }

    // @pre: network address of node
    // @post: creates a new NetConnection, ids it, and adds it to the registry
    void Router::addRemote(G3D::NetAddress& address){

        uint id = nonce++;

        remote_connection_t* cv = new remote_connection_t();
        cv->id = id;
        cv->handshake = false;

        // figure out later
        cv->y = 0;
        cv->h = 100;

        cv->connection = NetConnection::connectToServer(address, 1, UNLIMITED_BANDWIDTH, UNLIMITED_BANDWIDTH);
        remote_connection_registry[id] = cv;
    }

    void Router::removeRemote(G3D::NetAddress& address){
        // more complicated
    }

    void Router::broadcast(PacketType t, BinaryOutput& header, BinaryOutput& body, bool include_client){

        if(include_client) client->send(t, body, header, 0);

        map<uint, remote_connection_t*>::iterator iter;
        for(iter = remote_connection_registry.begin(); iter != remote_connection_registry.end(); iter++){ 
            iter->second->connection->send(t, body, header, 0);
        }
    }

}
