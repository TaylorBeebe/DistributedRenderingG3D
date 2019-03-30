#pragma once
#include "Node.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{
    class Remote : public Node::SingleConnectionNode{
        protected:
            uint startX, startY, endX, endY, width, height; 
            
            virtual void syncTransforms(TransformPacket* packet);
            virtual void renderFrameSegment(uint batch_id);
            
        public:
            Remote(int sx, int sy, int ex, int ey);
            
            void onData(uint socket_id, RenderPacket* packet) override; 
    }
}