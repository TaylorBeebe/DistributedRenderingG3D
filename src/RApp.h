#include "DistributedRenderer.h"
#include "Node.h"

using namespace DistributedRenderer;

namespace DistributedRenderer {
    class RApp : public GApp {
        protected:
            NetworkNode network_node;

        public:

            RApp();

            void onInit() override;

            virtual void onRun() override;
            virtual void oneFrame() override;
            virtual void oneFrameAdHoc(); 

            virtual void onCleanup() override;

            /** Sets m_endProgram to true. */
            virtual void endProgram();
    };
}