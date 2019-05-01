#include <G3D/G3D.h>
#include "../src/DistributedRenderer.h"

using namespace DistributedRenderer;
using namespace std;

int main(){

	cout << "Starting up..." << endl;

	initG3D();

	Remote remote (nullptr, true);

	remote.init_connection(Constants::ROUTER_ADDR);

	while (remote.isConnected()) {
		remote.receive();
	}

	cout << "Goodbye." << endl;

	return 0;
}