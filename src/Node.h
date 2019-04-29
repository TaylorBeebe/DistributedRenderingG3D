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
	};
}