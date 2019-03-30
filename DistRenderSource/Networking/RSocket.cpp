#include "RSocket.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

    void RSocket::sendPacket(RenderPacket* packet){
        send(*(packet->toBinary()));
    }

    bool RSocket::onConnect() {
        node->onConnectionReady(socket_id);
        return true;
    }


    void RSocket::onReady() {
        // Handshake with a new client
        // send("{\"type\": 0, \"value\":\"server ready\"");
        // clientWantsImage = 1;
    }

    bool RSocket::onData(Opcode opcode, char* data, size_t data_len) {

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
        
        // send it off to the node
        RenderPacket* packet = new RenderPacket(data, data_len, false);
        node->onData(socket_id, packet);

        return true;
    }

}
