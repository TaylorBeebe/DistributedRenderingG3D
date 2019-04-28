#pragma once
#include <G3D/G3D.h>
#include <map>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <string>
#include <array>

using namespace G3D;

// ********** COMPUTERS **********
// 1 -- 137.165.8.92
// 2 -- 137.165.8.62
// 3 -- 137.165.8.128 
// 4 -- 137.165.8.124
// 5 -- 137.165.209.29
// *******************************

namespace DistributedRenderer{

	class RApp {};

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

    // Connect to an address and store result in a NetConnection
    // @return: false if failed or timed out, true if successful
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

    // Easy conversion of data types to BinaryOutputs
    // @return: We must return a pointer to the newly contructed BinaryOutputs,
    // if we don't, the memory will be lost
    class BinaryUtils {
        public:
			
            // Make a simple, small "empty" packet for quick message sending
			static BinaryOutput* empty() {
				BinaryOutput* bo = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);
				bo->writeBool8(1);
				return bo;
			}

            // Write a single unsigned integer to a binary output
            static BinaryOutput* toBinaryOutput(uint32 i) {
                BinaryOutput* bo = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);
                bo->writeUInt32(i);
                return bo;
            }

            // Convery a BinaryInput to a BinaryOutput
            static BinaryOutput* toBinaryOutput(BinaryInput* in) {
				BinaryOutput* bo = new BinaryOutput("<memory>", G3DEndian::G3D_BIG_ENDIAN);

				// copy all bytes
				while (in->hasMore()) bo->writeInt8(in->readInt8());
				
                return bo;
            }
	};
}