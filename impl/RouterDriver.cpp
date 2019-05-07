#include <G3D/G3D.h>
#include "../src/Router.h"

using namespace std;
using namespace DistributedRenderer;
using namespace G3D;

int main(){

    // intialize G3D so we can use the networking library
	initG3D();

	Router::Router router;
    
    if(router.setup()) router.poll();

    router.terminate();

    cout << "Goodbye." << endl; 

    return 0;
}