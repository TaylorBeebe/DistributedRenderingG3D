#pragma once
#include "RemoteRenderer.h"
#include <map>
#include <G3D/G3D.h>

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

	class Client{
	    private:
	        unsigned int current_job_id = 0; 
	        float ms_to_deadline = 0;
	    public:
	        Client() {};

	        virtual void setDirtyBit(unsigned int id);
	        virtual void renderOnNetwork();
	};

}
