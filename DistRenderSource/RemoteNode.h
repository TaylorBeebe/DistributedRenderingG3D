#include "RemoteRenderer.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{
	class RemoteNode{	
		public:
			RemoteNode(){}

			virtual void sendFrameSegment();
			virtual void receiveUpdate(NetMessage* batch);	
			virtual void syncEntityTransform(unsigned int id, G3D::CFrame* frame);
	};
}
