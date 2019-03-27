#include "RemoteNode.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{

void RemoteNode::sendFrameSegment(){}

// @pre: registered ID and frame data of an entity
// @post: updates frame of corresponding entity with new position data
void RemoteNode::syncEntityTransform(unsigned int id, G3D::CFrame* frame){
	// safety check
	entityRegistry[id]->entity->setFrame(frame, true);
}

// @pre: encoded batch of transform updates
// @post: updates corresponding entity transforms
void RemoteNode::onData(G3D::BinaryInput& bistream){
	// for id,x,y,z,y,p,r in NetUtils::decode(batch):
	// 		G3D::CFrame*nframe = new G3D::CFrame();
	// 		nframe->fromXYZYPRRadians(x,y,z,y,p,r);
	//		syncEntityTransform(id, frame)
}

}
