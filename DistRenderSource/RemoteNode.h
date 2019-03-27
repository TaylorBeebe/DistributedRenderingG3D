#pragma once
#include "Node.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{
	class RemoteNode : public Node::SingleConnectionNode{
		protected:
			const int startX, startY;
			const int endX, endY; 	
		public:
			RemoteNode(int sx, int sy, int ex, int ey) : Node::SingleConnectionNode(), startX(sx), startY(sy), endX(ex), endY(ey) {}

			virtual void sendFrameSegment();
			virtual void syncEntityTransform(unsigned int id, G3D::CFrame* frame);
			
			void onData(G3D::BinaryInput& bitstream) override;	
	};
}
