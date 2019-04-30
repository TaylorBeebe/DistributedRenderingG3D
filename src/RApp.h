#pragma once
#include "DistributedRenderer.h"

namespace DistributedRenderer {

    class RApp : public GApp {

		private:

			/** Used by onWait for elapsed time. */
			RealTime               m_lastWaitTime;

			/** Seconds per frame target for the entire system \sa setFrameDuration*/
			float                  m_wallClockTargetDuration;

			/** \copydoc setLowerFrameRateInBackground */
			bool                   m_lowerFrameRateInBackground;

			/** SimTime seconds per frame, \see setFrameDuration, m_simTimeScale */
			float                  m_simTimeStep;
			float                  m_simTimeScale;
			float                  m_previousSimTimeStep;
			float                  m_previousRealTimeStep;

			RealTime               m_realTime;
			SimTime                m_simTime;

			ScreenCapture*                  m_screenCapture;

			OSWindow*                       m_window;

			bool                            m_hasUserCreatedWindow;
			bool                            m_hasUserCreatedRenderDevice;

			shared_ptr<Scene>               m_scene;

			SubmitToDisplayMode             m_submitToDisplayMode;

        protected:
            NetworkNode* network_node;

        public:

			RApp(const GApp::Settings& settings, NodeType type = REMOTE);

            virtual void onInit() override;

            void onRun();
            void oneFrame();
            virtual void oneFrameAdHoc(); 
			void onGraphics(RenderDevice* rd, Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) override;

            virtual void onCleanup() override;

            /** Sets m_endProgram to true. */
            void endProgram();
    };
}