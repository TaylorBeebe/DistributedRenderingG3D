#pragma once
#include "Node.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{
    class RemoteNode : public Node::SingleConnectionNode{
        protected:
            const uint startX, startY;
            const uint endX, endY;

            uint width;
            uint height; 
            
        public:
            RemoteNode(int sx, int sy, int ex, int ey) : Node::SingleConnectionNode(REMOTE), startX(sx), startY(sy), endX(ex), endY(ey) {
                width = endX - startX;
                height = endY - startY;
            }

            virtual void sendFrameSegment();
            virtual void syncEntityTransform(unsigned int id, G3D::CFrame* frame);
            
            void onData(uint socket_id, RenderPacket* packet) override; 
    };
}
