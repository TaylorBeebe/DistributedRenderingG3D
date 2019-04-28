#include <G3D/G3D.h>
#include "../src/Router.h"

using namespace std;
using namespace DistributedRenderer;
using namespace G3D;

int main(){

	initG3D(); // for networking
    Router::Router router; 
    if(router.setup()) poll();
    terminate();

    cout << "Goodbye." << endl; 

    return 0;
}