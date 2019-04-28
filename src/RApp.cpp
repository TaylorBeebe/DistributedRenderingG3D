/** \file Server.cpp

*/

#include "RApp.h"
//#include <time.h>
#include <mutex>


static const float BACKGROUND_FRAME_RATE = 4.0;

namespace DistributedRenderer {

	RApp::RApp(const GApp::Settings& settings, NodeType type) {
		// create a custom OSWindow
		OSWindow* window;
		// create custom RenderDevice
		RenderDevice* rd;


		//WARNING: This is downcasting because network_node is type NetworkNode. 
		//Check with Michael to see if there's a better way to go about this.

		// create node
		if (type == NodeType::CLIENT) network_node = Client(this);
		else network_node = Remote(this);

		G3D::GApp(settings, window, rd, true);


		//WARNING: No member "finishedSetup()"
		//if (type == NodeType::REMOTE) network_node.finishedSetup();
	}



	void DistributedRenderer::RApp::onInit()
	{
	}

	void RApp::onRun() {
		if (window()->requiresMainLoop()) { // this should never be free

			// The window push/pop will take care of
			// calling beginRun/oneFrame/endRun for us.
			window()->pushLoopBody(this);

		}
		else {
			beginRun();

			//debugAssertGLOk();

			// initialize the connection and wait for the ready
			network_node.init_connection();

			if (network_node.type == NodeType::CLIENT) {
				// Main loop
				do {
					oneFrame();
				} while (!m_endProgram);
			}
			else {
				// Busy wait for a message then do a render
				// This can be improved in the future (and should be) with thread sleeping
				do {


					//WARNING: This is downcasting. Check with Michael to see if there's a better way to go about this.
					((Remote)network_node).receive();
				
				
				} while (!m_endProgram);
			}

			endRun();
		}
	}

	// Similar to oneFrame, this method will call overridden app methods for pose
	// and graphics. This method will only be called by a remote node when it receives
	// network updates that request a render
	// 
	// the call to onGraphics will trigger whatever the developer specified in 
	// onGraphics2D and onGraphics3D. If a remote is in headless mode, draw requests
	// will be ignored on the render device
	void RApp::oneFrameAdHoc() {

		// Pose
		//BEGIN_PROFILER_EVENT("Pose");
		m_poseWatch.tick(); {
			m_posed3D.fastClear();
			m_posed2D.fastClear();
			onPose(m_posed3D, m_posed2D);

			// The debug camera is not in the scene, so we have
			// to explicitly pose it. This actually does nothing, but
			// it allows us to trigger the TAA code.
			m_debugCamera->onPose(m_posed3D);
		} m_poseWatch.tock();
		//END_PROFILER_EVENT();

		// Graphics
		//debugAssertGLOk();
		if ((submitToDisplayMode() == SubmitToDisplayMode::BALANCE) && (!renderDevice->swapBuffersAutomatically())) {
			swapBuffers();
		}

		//BEGIN_PROFILER_EVENT("Graphics");
		renderDevice->beginFrame();
		// m_widgetManager->onBeforeGraphics();
		m_graphicsWatch.tick(); {
			//debugAssertGLOk();
			renderDevice->pushState(); {
				//debugAssertGLOk();
				onGraphics(renderDevice, m_posed3D, m_posed2D);
			} renderDevice->popState();
		}  m_graphicsWatch.tock();
		renderDevice->endFrame();
		if ((submitToDisplayMode() == SubmitToDisplayMode::MINIMIZE_LATENCY) && (!renderDevice->swapBuffersAutomatically())) {
			swapBuffers();
		}
		//END_PROFILER_EVENT();

		// Remove all expired debug shapes
		for (int i = 0; i < debugShapeArray.size(); ++i) {
			if (debugShapeArray[i].endTime <= m_now) {
				debugShapeArray.fastRemove(i);
				--i;
			}
		}

		for (int i = 0; i < debugLabelArray.size(); ++i) {
			if (debugLabelArray[i].endTime <= m_now) {
				debugLabelArray.fastRemove(i);
				--i;
			}
		}

		debugText.fastClear();

		m_posed3D.fastClear();
		m_posed2D.fastClear();

		if (m_endProgram && window()->requiresMainLoop()) {
			window()->popLoopBody();
		}
	}

	void RApp::oneFrame() {

		RealTime timeStep = m_now - m_lastTime;

		for (int repeat = 0; repeat < std::max(1, m_renderPeriod); ++repeat) {
			Profiler::nextFrame();
			m_lastTime = m_now;
			m_now = System::time();
			

			// User input
			m_userInputWatch.tick();
			if (manageUserInput) {
				processGEventQueue();
			}
			onAfterEvents();
			onUserInput(userInput);
			m_userInputWatch.tock();

			// Network
			//BEGIN_PROFILER_EVENT("GApp::onNetwork");
			m_networkWatch.tick();
			onNetwork();
			m_networkWatch.tock();
			//END_PROFILER_EVENT();

			// Logic
			m_logicWatch.tick();
			{
				onAI();
			}
			m_logicWatch.tock();

			// Simulation
			m_simulationWatch.tick();
			//BEGIN_PROFILER_EVENT("Simulation");
			{
				RealTime rdt = timeStep;

				SimTime sdt = m_simTimeStep;
				if (sdt == MATCH_REAL_TIME_TARGET) {
					sdt = m_wallClockTargetDuration;
				}
				else if (sdt == REAL_TIME) {
					sdt = float(timeStep);
				}
				sdt *= m_simTimeScale;

				SimTime idt = m_wallClockTargetDuration;

				onBeforeSimulation(rdt, sdt, idt);
				onSimulation(rdt, sdt, idt);
				onAfterSimulation(rdt, sdt, idt);

				m_previousSimTimeStep = float(sdt);
				m_previousRealTimeStep = float(rdt);
				setRealTime(realTime() + rdt);
				setSimTime(simTime() + sdt);
			}
			m_simulationWatch.tock();
			//END_PROFILER_EVENT();
		}

		Client client = (Client)node;
		// after the simulation period, we will wait until our sands are run    
		RealTime deadline = timeStep + 100; // TODO: calculate something here with the given framerate and maybe borrow time from m_renderPeriod

		// send the update
		client.sendUpdate();

		while (timeStep <= deadline) client.checkNetwork();

		// Pose
		//BEGIN_PROFILER_EVENT("Pose");
		m_poseWatch.tick(); {
			m_posed3D.fastClear();
			m_posed2D.fastClear();
			onPose(m_posed3D, m_posed2D);

			// The debug camera is not in the scene, so we have
			// to explicitly pose it. This actually does nothing, but
			// it allows us to trigger the TAA code.
			m_debugCamera->onPose(m_posed3D);
		} m_poseWatch.tock();
		//END_PROFILER_EVENT();

		// Wait
		// Note: we might end up spending all of our time inside of
		// RenderDevice::beginFrame.  Waiting here isn't double waiting,
		// though, because while we're sleeping the CPU the GPU is working
		// to catch up.    
		//BEGIN_PROFILER_EVENT("Wait");
		m_waitWatch.tick(); {
			RealTime nowAfterLoop = System::time();

			// Compute accumulated time
			RealTime cumulativeTime = nowAfterLoop - m_lastWaitTime;

			//debugAssert(m_wallClockTargetDuration < finf());
			// Perform wait for actual time needed
			RealTime duration = m_wallClockTargetDuration;
			if (!window()->hasFocus() && m_lowerFrameRateInBackground) {
				// Lower frame rate
				duration = 1.0 / BACKGROUND_FRAME_RATE;
			}
			RealTime desiredWaitTime = std::max(0.0, duration - cumulativeTime);
			onWait(std::max(0.0, desiredWaitTime - m_lastFrameOverWait) * 0.97);

			// Update wait timers
			m_lastWaitTime = System::time();
			RealTime actualWaitTime = m_lastWaitTime - nowAfterLoop;

			// Learn how much onWait appears to overshoot by and compensate
			double thisOverWait = actualWaitTime - desiredWaitTime;
			if (std::abs(thisOverWait - m_lastFrameOverWait) / std::max(std::abs(m_lastFrameOverWait), std::abs(thisOverWait)) > 0.4) {
				// Abruptly change our estimate
				m_lastFrameOverWait = thisOverWait;
			}
			else {
				// Smoothly change our estimate
				m_lastFrameOverWait = lerp(m_lastFrameOverWait, thisOverWait, 0.1);
			}
		}  m_waitWatch.tock();
		//END_PROFILER_EVENT();

		// Graphics
		//debugAssertGLOk();
		if ((submitToDisplayMode() == SubmitToDisplayMode::BALANCE) && (!renderDevice->swapBuffersAutomatically())) {
			swapBuffers();
		}

		if (notNull(m_gazeTracker)) {
			//BEGIN_PROFILER_EVENT("Gaze Tracker");
			sampleGazeTrackerData();
			//END_PROFILER_EVENT();
		}

		//BEGIN_PROFILER_EVENT("Graphics");
		renderDevice->beginFrame();
		m_widgetManager->onBeforeGraphics();
		m_graphicsWatch.tick(); {
			//debugAssertGLOk();
			renderDevice->pushState(); {
				//debugAssertGLOk();
				onGraphics(renderDevice, m_posed3D, m_posed2D);
			} renderDevice->popState();
		}  m_graphicsWatch.tock();
		renderDevice->endFrame();
		if ((submitToDisplayMode() == SubmitToDisplayMode::MINIMIZE_LATENCY) && (!renderDevice->swapBuffersAutomatically())) {
			swapBuffers();
		}
		//END_PROFILER_EVENT();

		// Remove all expired debug shapes
		for (int i = 0; i < debugShapeArray.size(); ++i) {
			if (debugShapeArray[i].endTime <= m_now) {
				debugShapeArray.fastRemove(i);
				--i;
			}
		}

		for (int i = 0; i < debugLabelArray.size(); ++i) {
			if (debugLabelArray[i].endTime <= m_now) {
				debugLabelArray.fastRemove(i);
				--i;
			}
		}

		debugText.fastClear();

		m_posed3D.fastClear();
		m_posed2D.fastClear();

		if (m_endProgram && window()->requiresMainLoop()) {
			window()->popLoopBody();
		}
	}

	void RApp::onGraphics(RenderDevice* rd, Array<shared_ptr<Surface> >& posed3D, Array<shared_ptr<Surface2D> >& posed2D) {

		rd->pushState(); {
			//debugAssert(notNull(activeCamera()));
			rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
			onGraphics3D(rd, posed3D);
		} rd->popState();

		if (notNull(m_screenCapture)) {
			m_screenCapture->onAfterGraphics3D(rd);
		}

		// only draw 2D graphics can be drawn on the client
		if (network_node.type == NodeType::CLIENT) {

			rd->push2D(); {
				onGraphics2D(rd, posed2D);
			} rd->pop2D();

			if (notNull(m_screenCapture)) {
				m_screenCapture->onAfterGraphics2D(rd);
			}

		}
	}

	void RApp::onCleanup()
	{
	}

	void RApp::endProgram()
	{
	}

}

