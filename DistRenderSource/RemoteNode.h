#include "RemoteRenderer.h"
#include <G3D/G3D.h>

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
