#include <G3D/G3D.h>
#include "../src/DistributedRenderer.h"
#include "../src/Node.h"

using namespace DistributedRenderer;
using namespace std;

int main(){

	cout << "Starting up..." << endl;

	initG3D();

	RApp app; 
	Client client (app);

	client.init_connection(Constants::ROUTER_ADDR);

	while (client.isConnected()) client.checkNetwork();
	
	cout << "Goodbye." << endl;

	return 0;
}