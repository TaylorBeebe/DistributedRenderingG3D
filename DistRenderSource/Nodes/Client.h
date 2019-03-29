#pragma once
#include "Node.h"
#include <set>

using namespace std;
using namespace RemoteRenderer;

namespace RemoteRenderer{

    class Client : public Node::SingleConnectionNode{
        private:
            unsigned int current_job_id = 0; 
            float ms_to_deadline = 0;

            set<unsigned int> changed_entities;

        public:
            Client() : Node::SingleConnectionNode(CLIENT) {};
            
            virtual void setEntityChanged(unsigned int id);
            virtual void renderOnNetwork();

            void onData(uint socket_id, RenderPacket* packet) override;
    }
}
