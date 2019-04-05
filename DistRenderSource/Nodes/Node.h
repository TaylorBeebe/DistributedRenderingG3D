#pragma once
#include "RemoteRenderer.h"

using namespace G3D;

// Abstract definitions for network nodes
namespace RemoteRenderer{

    // NODE CLASS
    // defines behavior of a node on the network with hooks for sockets and the GApp
    class NetworkNode{
        protected:
            NodeType type; 

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
            NetworkNode(NodeType t, NetAddress& router_address) : type(t) {
                connection = NetConnection::connectToServer(router_address, 1, UNLIMITED_BANDWIDTH, UNLIMITED_BANDWIDTH);
            }

            // @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
            // @post: creates network ID for entity and stores reference to it
            // @returns: network ID of entity 
            uint registerEntity(Entity* e){
                entityRegistry[net_nonce] = e;
                return net_nonce++;
            } 

            bool isTypeOf(NodeType t){ return t == type; }
            bool isRunning() { return running; }
            
            // app hooks that will be called by the app
            virtual void onUpdate() {}
            virtual void onRender() {}
    }

    class Client : public NetworkNode{
        private:
            unsigned int current_batch_id = 0; 
            float ms_to_deadline = 0;

            set<unsigned int> changed_entities;
            NetMessageIterator iter;

            // frame cache

        public:
            Client();
            
            void setEntityChanged(unsigned int id);
            void sendTransforms();

            void checkNetwork();
    }

    class Remote : public NetworkNode{
        protected:
            Rect2D bounds; 

            bool received_screen_data = false;
            bool finished_setup = false;
            
            void syncTransforms(BinaryInput& packet);
            void sendFrame(uint batch_id);
            void setBounds(uint y, uint height);
            void maybeRegisterConfig();
            
        public:
            Remote();

            void finsihedUpdate();
            void receive();
    }
}