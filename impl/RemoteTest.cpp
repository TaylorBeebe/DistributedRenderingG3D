#include <G3D/G3D.h>
#include "../src/DistributedRenderer.h"
#include "../src/Node.h"

using namespace DistributedRenderer;
using namespace std;

int main(){

	cout << "Starting up..." << endl;

	initG3D();

	RApp app; 
	Remote remote (app, true);

	remote.init_connection(Constants::ROUTER_ADDR);

	while (remote.isConnected()) {
		remote.receive();
	}

	cout << "Goodbye." << endl;

	return 0;
}