#pragma once
#include "RemoteRenderer.h"

namespace RemoteRenderer{

    struct {
        bool handshake;
        uint id;
        uint y;
        uint h;
        shared_ptr<NetConnection> connection;
    } remote_connection_t;

    class Router {
        private: 
            bool running;
            
            // pixels
            uint current_batch;
            uint pieces;
            // -- some pixel buffer
            void flushPixelBuffer();
            void stitch(Image& fragment, uint x, uint y);
                
            // networking tools
            uint nonce;
            uint handshakes;
            map<uint, remote_connection_t*> remote_connection_registry;
            shared_ptr<NetworkConnection> client;

            void receive();
            void shutdown();

            void addClient(G3D::NetAddress& address);
            void addRemote(G3D::NetAddress& address);
            void removeRemote(G3D::NetAddress& address);

            void removeClient();

        public:
            Router();
    }

}


        