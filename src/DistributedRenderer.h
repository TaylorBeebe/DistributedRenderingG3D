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
using namespace std;

// ********** COMPUTERS **********
// 1 -- 137.165.8.92
// 2 -- 137.165.8.62
// 3 -- 137.165.8.128 
// 4 -- 137.165.8.124
// 5 -- 137.165.209.29
// *******************************

namespace DistributedRenderer {

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

			static BinaryOutput* create() {
				return new BinaryOutput("<memory>", G3DEndian::G3D_LITTLE_ENDIAN);
			}

            // Make a simple, small "empty" packet for quick message sending
			static BinaryOutput* empty() {
				BinaryOutput* bo = BinaryUtils::create();
				bo->writeBool8(1);
				return bo;
			}

            // Write a single unsigned integer to a binary output
            static BinaryOutput* toBinaryOutput(uint32 i) {
                BinaryOutput* bo = BinaryUtils::create();
                bo->writeUInt32(i);
                return bo;
            }

            // Convert a BinaryInput to a BinaryOutput
            static BinaryOutput* toBinaryOutput(BinaryInput* in) {
				BinaryOutput* bo = BinaryUtils::create();

				// copy all bytes
				while (in->hasMore()) bo->writeInt8(in->readInt8());
				
                return bo;
            }

            static BinaryOutput* copy(BinaryOutput* out) {
                BinaryOutput* bo = BinaryUtils::create();

                // copy all bytes
				const uint8* buffer = out->getCArray();
				for (int i = 0; i < out->length(); i++) {
					bo->writeUInt8(buffer[i]);
				}

                return bo;
            }
	};

    // =========================================
    //             Class Definitions
    // =========================================

	class RApp;
	class NetworkNode;

    // NODE CLASS
    class NetworkNode{

        protected:
            NodeType type; 

            bool headless;

            RApp* the_app;
            // <- render device

            // Each entity in the scene will have a registered network ID
            // which should be the same over all instances of the application 
            // then at runtime, transforms will be synced across the network
            // before rendering a frame
            Array<shared_ptr<Entity>> entities;
            map<String, uint32> entity_index_by_name;

            shared_ptr<NetConnection> connection; 

            void send(PacketType t, BinaryOutput& header, BinaryOutput& body){
                connection->send(t, body, header, 0);
            }

            // send empty packet with type
            void send(PacketType t){
                connection->send(t, *BinaryUtils::empty(), 0);
            }

            virtual void onConnect() {}

        public:
            NetworkNode(NodeType t, RApp* app, bool head) : type(t), the_app(app), headless(head) {}

            bool init_connection(NetAddress router_address) {
                
                // TODO: check that the connection wasn't already initialized

                if (connect(router_address, &connection)) {
                    onConnect();
                    return true;
                } else cout << "Connection Failed" << endl;

                return false;
            }

            bool isTypeOf(NodeType t){ return t == type; }

            bool isConnected() { 
                NetConnection::NetworkStatus status = connection->status();
                return connection != NULL && (status == NetConnection::CONNECTED || status == NetConnection::JUST_CONNECTED); 
            }

            bool isHeadless() { return headless; }

            // register all entities to be tracked by the network 
            // currently, this does not support adding or removing entities
            void trackEntities(Array<shared_ptr<Entity>>* e) {
                entities = *e;
                // make an ID lookup for tracking
                for(int i = 0; i < entities.size(); i++){
                    entity_index_by_name[entities[i]->name()] = i;
                }
            }

            uint32 getEntityIDByName(String name){
                return entity_index_by_name[name];
            }

            shared_ptr<Entity> getEntityByID(uint32 id){
                return entities[id];
            }

    };

    class Client : public NetworkNode{
        protected:
            uint32 current_batch_id = 0; 
            float ms_to_deadline = 0;

            set<unsigned int> changed_entities;

            // frame cache

            void onConnect() override;

        public:
            Client(RApp* app);
            
            void setEntityChanged(shared_ptr<Entity> e);
            void sendUpdate();

            void checkNetwork();

    };

    class Remote : public NetworkNode{
        protected:
            Rect2D bounds; 
            
            void sync(BinaryInput& update);
            void sendFrame(uint32 batch_id);

            void setClip(BinaryInput& bi);
            void setClip(uint32 y, uint32 height);
            
            void onConnect() override;

        public:
            Remote(RApp* app, bool headless_mode);
            void receive();
            Rect2D getBounds() { return bounds; }
    };

    class RApp : public GApp {

        private:
			
            /** Used by onWait for elapsed time. */
            RealTime               r_lastWaitTime;

        protected:
            NetworkNode* network_node;

        public:
            RApp(const GApp::Settings& settings, NodeType type = REMOTE);

            virtual void onInit() override;
		
			int run();
            void onRun();
            void oneFrame();
            virtual void oneFrameAdHoc(); 
            void onGraphics(RenderDevice* rd, Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) override;

            virtual void onCleanup() override;

            /** Sets m_endProgram to true. */
            void endProgram();
    };

}