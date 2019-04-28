#pragma once
#include <G3D/G3D.h>
#include <map>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <string>

using namespace G3D;

class RApp;

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
        static const RealTime CONNECTION_WAIT = 10;
        static const bool COMPRESS_NETWORK_DATA = false;

        static const uint16 PORT = 8080; // node port

        static NetAddress ROUTER_ADDR ("137.165.8.92", PORT);

        //static NetAddress CLIENT_ADDR ("137.165.209.29", PORT); 

        // static NetAddress N1_ADDR ("137.165.8.62", PORT);
        // static NetAddress N2_ADDR ("137.165.8.128", PORT); 
        // static NetAddress N3_ADDR ("137.165.8.124", PORT); 

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
        HI_AM_CLIENT
    };

    // =========================================
    //                   Utils
    // =========================================

    // wait on a connection
    static bool connect(NetAddress& addr, shared_ptr<NetConnection>* conn){

		try {

			shared_ptr<NetConnection> connection;

			connection = NetConnection::connectToServer(addr, 1, NetConnection::UNLIMITED_BANDWIDTH, NetConnection::UNLIMITED_BANDWIDTH);

			RealTime deadline = System::time() + Constants::CONNECTION_WAIT;
			while (connection->status() == NetConnection::NetworkStatus::WAITING_TO_CONNECT && System::time() < deadline) {}

			if (connection->status() == NetConnection::NetworkStatus::JUST_CONNECTED) {
				*conn = connection;
				return true;
			}

		} catch (...) { }

		return false;
    }

    // easy conversion of data types to BinaryOutputs
    class BinaryUtils {
        public:
				
			static BinaryOutput* empty() {
				BinaryOutput* bo = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);
				bo->writeBool8(1);
				return bo;
			}

            static BinaryOutput* toBinaryOutput(uint32 i) {
                BinaryOutput* bo = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);
                bo->writeUInt32( i);
                return bo;
            }

            static BinaryOutput* toBinaryOutput(uint32 list[]) {
				BinaryOutput* bo = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);;

				int length = sizeof(list) / sizeof(uint32);
				for (int i = 0; i < length; i++) bo->writeUInt32(list[i]);
                
                return bo;
            }

            static BinaryOutput* toBinaryOutput(BinaryInput* in) {
				BinaryOutput* bo = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);

				// copy all bytes
				while (in->hasMore()) bo->writeInt8(in->readInt8());
				
                return bo;
            }
	};
}