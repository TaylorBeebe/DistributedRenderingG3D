#include "DistributedRenderer.h"

using namespace DistributedRenderer;

int main(){

	RApp app = new RApp();

	Remote remote (app, true);

	while (remote.isRunning()) {
		remote.receive();
	}

	cout << "Goodbye." << endl;

	return 0;
}