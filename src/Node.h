#pragma once
#include "DistributedRenderer.h"

using namespace G3D;
using namespace std;

// Abstract definitions for network nodes
namespace DistributedRenderer{

    // NODE CLASS
    class NetworkNode{
        protected:
            NodeType type; 

			bool headless;

            RApp& the_app;
            // <- render device

            // Each entity in the scene will have a registered network ID
            // which will be synced across the network at setup so that at 
            // runtime transform data can be synced 
            map<uint32, Entity*> entityRegistry;

            // maybe use a unique id later
            uint net_nonce = 0;

            shared_ptr<NetConnection> connection; 

            void send(PacketType t, BinaryOutput& header, BinaryOutput& body){
                connection->send(t, body, header, 0);
            }

            // send empty packet with type
            void send(PacketType t){
				//BinaryOutput bo("<memory>", G3DEndian::G3D_BIG_ENDIAN);
				//bo.writeBool8(1);
                connection->send(t, *BinaryUtils::empty(), 0);
            }

			virtual void onConnect() {}

        public:
            NetworkNode(NodeType t, RApp& app, bool head) : type(t), the_app(app), headless(head) {}

			bool init_connection(NetAddress router_address) {
				
				// TODO: check that the connection wasn't already initialized

				if (connect(router_address, &connection)) {
					onConnect();
					return true;
				} else cout << "Connection Failed" << endl;

				return false;
			}

            // @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
            // @post: creates network ID for entity and stores reference to it
            // @returns: network ID of entity 
            uint32 registerEntity(Entity* e){
                entityRegistry[net_nonce] = e;
                return net_nonce++;
            } 

            bool isTypeOf(NodeType t){ return t == type; }

            bool isConnected() { 
				NetConnection::NetworkStatus status = connection->status();
				return connection != NULL && (status == NetConnection::CONNECTED || status == NetConnection::JUST_CONNECTED); 
			}
			bool isHeadless() { return headless; }
	};

    class Client : public NetworkNode{
        protected:
            uint32 current_batch_id = 0; 
            float ms_to_deadline = 0;

            set<unsigned int> changed_entities;

            // frame cache

			void onConnect() override;

        public:
            Client(RApp& app);
            
            void setEntityChanged(uint32 id);
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
            Remote(RApp& app, bool headless_mode);
            void receive();
	};
}