#pragma once
#include <G3D/G3D.h>
#include <map>
#include <vector>
#include <list>
#include <set>

namespace RemoteRenderer{

    namespace Constants {

        // display
        const uint FRAMERATE = 30;
        
        const uint SCREEN_WIDTH = 1920;
        const uint SCREEN_HEIGHT = 1080;

        // networking
        const bool COMPRESS_NETWORK_DATA = false;

        const uint16 RPORT = 1100;
        const uint16 APORT = 9000;

        const G3D::NetAddress ROUTER_ADDR (101010101, RPORT);
        const G3D::NetAddress CLIENT_ADDR (101010101, APORT);
        const G3D::NetAddress N1_ADDR (101010101, APORT);
        const G3D::NetAddress N2_ADDR (101010101, APORT);
        const G3D::NetAddress N3_ADDR (101010101, APORT);
    }

    enum NodeType {
        CLIENT,
        REMOTE
    }

    // Supported network packet types
    enum PacketType {
        TRANSFORM,
        FRAME,
        FRAGMENT,
        HANDSHAKE,
        READY,
        END
    }


    // Utils
    
    BinaryOutput& toBinaryOutput(uint i){
        BinaryOutput bo ();
        bo.setEndian(G3DEndian::G3D_BIG_ENDIAN);

        bo.beginBits();
        bo.writeUInt32(i);
        bo.endBits();

        return bo;
    }

    BinaryOutput& toBinaryOutput(BinaryInput& in){
        BinaryOutput bo ();
        bo.setEndian(G3DEndian::G3D_BIG_ENDIAN);

        bo.beginBits();
        bo.writeBits((uint) in.getCArray(), in.getLength());
        bo.endBits();

        return bo;
    }

}