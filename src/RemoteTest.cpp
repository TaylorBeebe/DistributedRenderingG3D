#include "DistributedRenderer.h"

using namespace DistributedRenderer;
using namespace std;

int main(){

	cout << "Starting up..." << endl;

	RApp app = new RApp();

	Remote remote (app, true);

	cout << "Node initialized, now listening..." << endl;

	while (remote.isRunning()) {
		remote.receive();
	}

	cout << "Goodbye." << endl;

	return 0;
}