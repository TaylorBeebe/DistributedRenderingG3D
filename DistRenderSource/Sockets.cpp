#include "Sockets.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

bool SingleSocket::onConnect() {
    return true;
}


void SingleSocket::onReady() {
    // Handshake with a new client
    // send("{\"type\": 0, \"value\":\"server ready\"");
    // clientWantsImage = 1;
}


bool SingleSocket::onData(Opcode opcode, char* data, size_t data_len) {

    // Program currently ignores anything not BINARY
    if (opcode != BINARY) return true;

    if ((data_len == 6) && (memcmp(data, "\"ping\"", 6) == 0)) {
        // This is our application protocol ping message; ignore it
        return true;
    }

    if ((data_len < 2)) {
        // Some corrupt message
        debugPrintf("Message makes no sense\n");
        return true;
    }

    // pass on bitstream
    G3D::BinaryInput* bitstream = new G3D::BinaryInput(data, data_len, G3D::G3DEndian::G3D_BIG_ENDIAN, false, true);
    node->onData(bitstream);

    return true;
}

}
