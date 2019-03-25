#include "Utils.h"

//class which holds graphics info
class GraphicsPacket {
public:
	
	//current frame id
	int frame_id;
	
	shared_ptr<NetConnection> connection;

	//put variable for 2D array of pixels.
	//WHAT IS THE SIZE OF A PIXEL?

	GraphicsPacket(){
		//construction
	}

	~GraphicsPacket() {
		//deconstruction
	}
};

//holds info sent from client and echoed from server to generate a frame
class UpdatePacket {
public:

	//current frame id
	int frame_id;

	//see NetConnection class
	shared_ptr<NetConnection> connection;

	//put variable for 2D array of pixels.
	//WHAT IS THE SIZE OF A PIXEL?

	UpdatePacket() {
		//construction
	}

	~UpdatePacket() {
		//deconstruction
	}
};