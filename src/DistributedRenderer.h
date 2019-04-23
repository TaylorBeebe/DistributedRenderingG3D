#pragma once
#include <G3D/G3D.h>
#include <map>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <string>

using namespace G3D;

// ********** COMPUTERS **********
// 1 -- 137.165.8.92
// 2 -- 137.165.8.62
// 3 -- 137.165.8.128 
// 4 -- 137.165.8.124
// 5 -- 137.165.209.29
// *******************************

namespace DistributedRenderer{

    namespace Constants {

        // display
        static const uint32 FRAMERATE = 30;
        
        static const uint32 SCREEN_WIDTH = 1920;
        static const uint32 SCREEN_HEIGHT = 1080;

        static const uint32 PIXEL_BLEED = 100;

        // networking
        static const RealTime CONNECTION_WAIT = 20;
        static const bool COMPRESS_NETWORK_DATA = false;

        static const uint16 PORT = 1000; // node port

        static NetAddress ROUTER_ADDR ("137.165.8.92", PORT); // 1
        static NetAddress CLIENT_ADDR ("137.165.209.29", PORT); // 5

        static NetAddress N1_ADDR ("137.165.8.62", PORT); // 2
        static NetAddress N2_ADDR ("137.165.8.128", PORT); // 3
        static NetAddress N3_ADDR ("137.165.8.124", PORT); // 4

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
        TERMINATE,
        HI_AM_REMOTE,
        HI_AM_CLIENT,
        ACK
    };

    // =========================================
    //                   Utils
    // =========================================

    // wait on a connection
    static bool connect(NetAddress& addr, shared_ptr<NetConnection> conn){
		try {
			conn = NetConnection::connectToServer(addr, 1, NetConnection::UNLIMITED_BANDWIDTH, NetConnection::UNLIMITED_BANDWIDTH);
		} catch (...) { return false; }

		RealTime deadline = System::time() + Constants::CONNECTION_WAIT;
        while (conn->status() == NetConnection::NetworkStatus::WAITING_TO_CONNECT && System::time() < deadline) {}
        return conn->status() == NetConnection::NetworkStatus::CONNECTED;
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