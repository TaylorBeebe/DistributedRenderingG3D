#pragma once
#include "RemoteRenderer.h"

using namespace RemoteRenderer;

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
            map<unsigned int, G3D::Entity*> entityRegistry;

            // maybe use a unique id later
            uint net_nonce = 0;

            bool running = false;

            shared_ptr<G3D::NetConnection> connection; 
            G3D::NetMessageIterator iter;

            // send a pakcet via the connection
            void send(RenderPacket& packet) {
                connection->send(packet.getPacketType(), packet.serializeBody(), packet.serializeHeader(), 0);
            }

            // send empty packet
            void send(PacketType t){
                // intialize empty binary
                G3D::BinaryOutput bo ();
                bo.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

                connection->send(t, bo, 0);
            }

        public:
            NetworkNode(NodeType t, G3D::NetAddress& router_address) : type(t) {
                connection = G3D::NetConnection::connectTorouter(router_address, 1, G3D::UNLIMITED_BANDWIDTH, G3D::UNLIMITED_BANDWIDTH);
                iter = connection->incomingMessageIterator();
            }

            // @pre: expects pointer to Entity or subclass of Entity (cast as Entity)
            // @post: creates network ID for entity and stores reference to it
            // @returns: network ID of entity 
            uint registerEntity(Entity* e){
                entityRegistry[net_nonce] = e;
                return net_nonce++;
            } 

            bool isTypeOf(NodeType t){ return t == type; }

            void readNetworkUpdate() {
                if(iter.isValid()){
                    // make a render packet
                    RenderPacket* packet = new RenderPacket((PacketType) iter.type(), iter.headerBinaryInput(), iter.binaryInput());
                    
                    // call network node packet handler
                    onData(packet);

                    // pop the message off of the queue
                    iter++;
                }
            }

            void connectionReady(){
                // tell the router the node is ready
                send(PacketType::HANDSHAKE);
            }

            // app hooks that will be called by the app
            virtual void onUpdate() {}
            virtual void onRender() {}

            // network hooks called in response to network requests
            virtual void onData(RenderPacket& packet) {}
    }

    class Client : public NetworkNode{
        private:
            unsigned int current_batch_id = 0; 
            float ms_to_deadline = 0;

            set<unsigned int> changed_entities;

            // frame cache

        public:
            Client();
            
            void setEntityChanged(unsigned int id);
            void sendTransforms();

            void onData(RenderPacket& packet) override;
    }

    class Remote : public NetworkNode{
        protected:
            G3D::Rect2D bounds; 

            bool received_screen_data = false;
            bool finished_setup = false;
            
            void syncTransforms(RenderPacket& packet);
            void sendFrame(uint batch_id);
            
        public:
            Remote();

            void setBounds(float sx, float sy, float ex, float ey);

            void waitForNextUpdate() {
                // busy wait until there's a message (room for opitimization with thread sleeping but only with a custom networking model)
                // network listeners run on a separate network thread and will add to the iter queue in a threadsafe fashion
                while(!iter.isValid()) {}
                
                readNetworkUpdate();   
            }

            void maybeRespondHandshake();

            void onData(RenderPacket& packet) override; 
    }
}