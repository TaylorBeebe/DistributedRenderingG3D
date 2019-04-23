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

            bool running;

            shared_ptr<NetConnection> connection; 

            void send(PacketType t, BinaryOutput& header, BinaryOutput& body){
                connection->send(t, body, header, 0);
            }

            // send empty packet with type
            void send(PacketType t){
                connection->send(t, BinaryUtils::empty(), BinaryUtils::empty(), 0);
            }

            virtual void onConnect() {}

        public:
            NetworkNode(NodeType t, NetAddress& router_address, RApp& app, bool head) : type(t), the_app(app), headless(head), running(false) {
                if(!connect(router_address, connection)){
                    cout << "Could not connect, shutting down..." << endl;
                    // the_app.end() or something
				}else{
                    running = true;
                    onConnect();
                }
            }

            // @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
            // @post: creates network ID for entity and stores reference to it
            // @returns: network ID of entity 
            uint32 registerEntity(Entity* e){
                entityRegistry[net_nonce] = e;
                return net_nonce++;
            } 

            bool isTypeOf(NodeType t){ return t == type; }
            bool isRunning() { return running; }
			bool isHeadless() { return headless; }
	};

    class Client : public NetworkNode{
        private:
            unsigned int current_batch_id = 0; 
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