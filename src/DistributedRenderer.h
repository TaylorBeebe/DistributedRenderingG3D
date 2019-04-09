#pragma once
#include <G3D/G3D.h>
#include <map>
#include <vector>
#include <list>
#include <set>

using namespace G3D;

namespace DistributedRenderer{

    namespace Constants {

        // display
        static const uint32 FRAMERATE = 30;
        
        static const uint32 SCREEN_WIDTH = 1920;
        static const uint32 SCREEN_HEIGHT = 1080;

        static const uint32 PIXEL_BLEED = 100;

        // networking
        static const RealTime CONNECTION_WAIT = 10;
        static const bool COMPRESS_NETWORK_DATA = false;

        static const uint16 RPORT = 1100;
        static const uint16 APORT = 9000;

        static NetAddress ROUTER_ADDR (101010101, RPORT);
        static NetAddress CLIENT_ADDR (101010101, APORT);
        static NetAddress N1_ADDR (101010101, APORT);
        static NetAddress N2_ADDR (101010101, APORT);
        static NetAddress N3_ADDR (101010101, APORT);

    }

	enum NodeType {
		CLIENT,
		REMOTE
	};

    // Supported network packet types
    enum PacketType {
        UPDATE,
        FRAME,
        FRAGMENT,
        CONFIG,
        CONFIG_RECEIPT,
        READY,
        TERMINATE
    };

    // =========================================
    //                   Utils
    // =========================================

    // wait on a connection
    static bool connect(NetAddress& addr, shared_ptr<NetConnection> conn){
        conn = NetConnection::connectToServer(addr, 1, NetConnection::UNLIMITED_BANDWIDTH, NetConnection::UNLIMITED_BANDWIDTH);
        RealTime deadline = System::time() + Constants::CONNECTION_WAIT;
        while (conn->status() == NetConnection::NetworkStatus::WAITING_TO_CONNECT && System::time() < deadline) {}
        return conn->status() == NetConnection::NetworkStatus::JUST_CONNECTED;
    }

	class RApp {
		public:
			int a = 1;
	};

    // easy conversion of data types to BinaryOutputs
    class BinaryUtils {
        public:
				
			static BinaryOutput& empty() {
				return BinaryUtils::toBinaryOutput((uint32) 0);
			}

            static BinaryOutput& toBinaryOutput(uint32 i) {
                BinaryOutput bo ("<memory>", G3DEndian::G3D_BIG_ENDIAN);

                bo.beginBits();
                bo.writeUInt32(i);
                bo.endBits();

                return bo;
            }

            static BinaryOutput& toBinaryOutput(uint32 list[]) {
				BinaryOutput bo("<memory>", G3DEndian::G3D_BIG_ENDIAN);;
                bo.setEndian(G3DEndian::G3D_BIG_ENDIAN);

				bo.beginBits();

				int length = sizeof(list) / sizeof(uint32);
				for (int i = 0; i < length; i++) bo.writeUInt32(list[i]);
                
                bo.endBits();

                return bo;
            }

            static BinaryOutput& toBinaryOutput(BinaryInput& in) {
				BinaryOutput bo("<memory>", G3DEndian::G3D_BIG_ENDIAN);
                bo.setEndian(G3DEndian::G3D_BIG_ENDIAN);

                bo.beginBits();
                bo.writeBits((uint32) in.getCArray(), in.getLength());
                bo.endBits();

                return bo;
            }
	};
}