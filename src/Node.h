#pragma once
#include "DistributedRenderer.h"

using namespace G3D;

// Abstract definitions for network nodes
namespace DistributedRenderer{

    // NODE CLASS
    class NetworkNode{
        protected:
            NodeType type; 

            RApp& the_app;
            // render device

            // Each entity in the scene will have a registered network ID
            // which will be synced across the network at setup so that at 
            // runtime transform data can be synced 
            map<unsigned int, Entity*> entityRegistry;

            // maybe use a unique id later
            uint net_nonce = 0;

            bool running = false;

            shared_ptr<NetConnection> connection; 

            void send(PacketType t, BinaryOutput& header, BinaryOutput& body){
                connection->send(t, body, header, 0);
            }

            // send empty packet with type
            void send(PacketType t){
                connection->send(t, serializeUInt(0), serializeUInt(0), 0);
            }

        public:
            NetworkNode(NodeType t, NetAddress& router_address, RApp& app) : type(t), the_app(app) {
                if(!connect(router_address, connection)) return; // something bad happened, TODO: end program
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

            virtual void finishedSetup() {}
	};

    class Client : public NetworkNode{
        private:
            unsigned int current_batch_id = 0; 
            float ms_to_deadline = 0;

            set<unsigned int> changed_entities;

            // frame cache

        public:
            Client(RApp& app);
            
            void setEntityChanged(unsigned int id);
            void sendTransforms();

            void checkNetwork();
	};

    class Remote : public NetworkNode{
        protected:
            bool headless;
            Rect2D bounds; 
            
            void syncTransforms(BinaryInput& packet);
            void sendFrame(uint batch_id);

            void setClip(uint y, uint height);
            
        public:
            Remote(RApp& app, bool h);
            void receive();
	};
}