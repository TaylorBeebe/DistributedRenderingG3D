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

	if(remote.isRunning()) cout << "Node initialized, now listening..." << endl;
	while (remote.isRunning()) {
		remote.receive();
	}

	cout << "Goodbye." << endl;

	return 0;
}