#pragma once
#include "RemoteRenderer.h"

namespace RemoteRenderer{

    class Router {
        private: 
            bool running = false;
            uint current_batch;
            uint num_nodes = 0;

            shared_ptr<G3D::NetServer> net_server;

            // pixels
            uint pieces = 0;
            // some pixel buffer
            void flushPixelBuffer();
            void stitch(Image& fragment, uint x, uint y);
                
            // networking tools
            void poll();
            void shutdown();

        public:
            Router();

            void addConnectionClient(G3D::NetAddress& address);
            void addConnectionRemote(G3D::NetAddress& address);

            void onData(RenderPacket& packet);
    }

}


        