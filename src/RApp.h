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

            void onRun();
            void oneFrame();
            virtual void oneFrameAdHoc(); 
			void onGraphics(RenderDevice* rd, Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) override;

            virtual void onCleanup() override;

            /** Sets m_endProgram to true. */
            virtual void endProgram();
    };
}