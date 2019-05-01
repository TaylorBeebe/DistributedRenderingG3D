#include <G3D/G3D.h>
#include "../src/DistributedRenderer.h"

using namespace DistributedRenderer;
using namespace std;

int main(){

	cout << "Starting up..." << endl;

	initG3D();

	Client client (nullptr);

	client.init_connection(Constants::ROUTER_ADDR);

	while (client.isConnected()) client.checkNetwork();
	
	cout << "Goodbye." << endl;

	return 0;
}