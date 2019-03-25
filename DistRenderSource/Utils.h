#pragma once
#include <G3D/G3D.h>

class GraphicsPacket {

public:

	//current frame id
	int frame_id;

	shared_ptr<NetConnection> connection;

	//need pixel array data
};

//holds info sent from client and echoed from server to generate a frame
class UpdatePacket {

public:

	//current frame id
	int frame_id;

	//see NetConnection class
	shared_ptr<NetConnection> connection;

	//others

};