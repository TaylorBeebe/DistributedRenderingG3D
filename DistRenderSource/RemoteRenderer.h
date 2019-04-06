#pragma once
#include <G3D/G3D.h>
#include <map>
#include <vector>
#include <list>
#include <set>

namespace RemoteRenderer{

    namespace Constants {

        // display
        const uint32 FRAMERATE = 30;
        
        const uint32 SCREEN_WIDTH = 1920;
        const uint32 SCREEN_HEIGHT = 1080;

        const uint32 PIXEL_BLEED = 100;

        // networking
        const bool COMPRESS_NETWORK_DATA = false;

        const uint16 RPORT = 1100;
        const uint16 APORT = 9000;

        const NetAddress ROUTER_ADDR (101010101, RPORT);
        const NetAddress CLIENT_ADDR (101010101, APORT);
        const NetAddress N1_ADDR (101010101, APORT);
        const NetAddress N2_ADDR (101010101, APORT);
        const NetAddress N3_ADDR (101010101, APORT);

    }

	enum NodeType {
		CLIENT,
		REMOTE
	};

    // Supported network packet types
    enum PacketType {
        TRANSFORM,
        FRAME,
        FRAGMENT,
        CONFIG,
        CONFIG_RECEIPT
        READY,
        TERMINATE
    }

    // Utils
    class BinaryUtils {
        public:
            static BinaryOutput& toBinaryOutput(uint i) {
                BinaryOutput bo ();
                bo.setEndian(G3DEndian::G3D_BIG_ENDIAN);

                bo.beginBits();
                bo.writeUInt32(i);
                bo.endBits();

                return bo;
            }

            static BinaryOutput& toBinaryOutput(uint list[]) {
                BinaryOutput bo ();
                bo.setEndian(G3DEndian::G3D_BIG_ENDIAN);

                for(uint i : list){
                    bo.writeUInt32(i);
                }

                bo.endBits();

                return bo;
            }

            static BinaryOutput& toBinaryOutput(BinaryInput& in) {
                BinaryOutput bo ();
                bo.setEndian(G3DEndian::G3D_BIG_ENDIAN);

                bo.beginBits();
                bo.writeBits((uint) in.getCArray(), in.length());
                bo.endBits();

                return bo;
            }
	};
}