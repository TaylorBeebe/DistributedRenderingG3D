#include "RSocket.h"

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{


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

        // decide what kind of packet the data needs to be
        RenderPacket* packet;

        // pass on bitstream
        G3D::BinaryInput* bitstream = new G3D::BinaryInput(data, data_len, G3D::G3DEndian::G3D_BIG_ENDIAN, false, true);

        // read the preamble
        bitstream->beginBits();
        uint batch_id = bitstream->readUInt32();
        uint type = bitstream->readUInt32();
        bitstream->endBits();

        // if the node is a server, send a raw packet because we don't care about the raw data
        // otherwise convert it to a much more complex data packet depending on the type
        if(node->isTypeOf(SERVER)){
            packet = (RenderPacket*) new RawPacket(batch_id, type, bitstream, false);
        }else{
            switch(type){
                case TRANSFORM:
                    packet = (RenderPacket*) new TransformPacket(batch_id, bitstream);
                    break;
                case FRAME:
                    packet = (RenderPacket*) new FramePacket(batch_id, bitstream);
                    break;
            }
        }

        // send it off to the node
        node->onData(socket_id, packet);

        return true;
    }

}
