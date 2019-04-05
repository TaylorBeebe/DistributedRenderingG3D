#pragma once
#include "RemoteRenderer.h"

namespace RemoteRenderer{

    struct {
        bool configured;
        uint id;
        uint y;
        uint h;
        shared_ptr<NetConnection> connection;
    } remote_connection_t;

    class Router {
        private: 
            // app is running
            bool running;
            
            // PIXELS
            uint current_batch;
            uint pieces;
            // -- some pixel buffer
            void flushPixelBuffer();
            void stitch(Image& fragment, uint x, uint y);

            void configureScreenSplit();
                
            // NETWORKING
            uint nonce; // for basic, fast remote identifiers
            uint configurations; // internal tally of configured remotes

            // registry of remote nodes
            map<uint, remote_connection_t*> remote_connection_registry;
            // the client connection
            shared_ptr<NetworkConnection> client;

            // packet reception listening and handling
            void receive();
            
            void addClient(G3D::NetAddress& address);
            void addRemote(G3D::NetAddress& address);
            void removeRemote(G3D::NetAddress& address);

            void shutdown();

        public:
            Router();
    }

}


        