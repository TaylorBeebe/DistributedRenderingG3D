/** \file Server.cpp

*/

#include "DistributedRenderer.h"
#include "FramebufferDist.h"
#include <mutex>

static const float BACKGROUND_FRAME_RATE = 4.0;



namespace DistributedRenderer {

	OSWindow* os_window = nullptr;
	RenderDevice* render_device = nullptr;

	static void createRenderDevice(const GApp::Settings& settings) {
		render_device = RenderDeviceDist::create(settings);
		os_window = render_device->window();
	}

	static OSWindow* getConstructorOSWindow(const GApp::Settings& settings, NodeType type) {

		if (notNull(os_window)) {

			return os_window;
		}
		else if (type == NodeType::CLIENT) {

			return nullptr;

		}
		else {

			createRenderDevice(settings);
			return os_window;
		}

	}

	static RenderDevice* getConstructorRenderDevice(const GApp::Settings& settings, NodeType type) {

		if (type == NodeType::CLIENT) {

			return nullptr;

		}
		else {

			createRenderDevice(settings);
			return render_device;
		}

	}

	//OSWindow::create(settings.window)
	RApp::RApp(const GApp::Settings& settings, NodeType type) : 
		GApp(settings, getConstructorOSWindow(settings,type), getConstructorRenderDevice(settings,type), true), 
		r_lastWaitTime(System::time()) 
	{
		// create node
		if (type == NodeType::CLIENT) network_node = new Client(this);
		else network_node = new Remote(this, true);
	}

	void DistributedRenderer::RApp::onInit(){
		GApp::onInit();
	}

	int RApp::run() {
		int ret = 0;
		if (catchCommonExceptions) {
			try {
				onRun();
				ret = m_exitCode;
			}
			catch (const char* e) {
				alwaysAssertM(false, e);
				ret = -1;
			}
			catch (const Image::Error& e) {
				alwaysAssertM(false, e.reason + "\n" + e.filename);
				ret = -1;
			}
			catch (const String& s) {
				alwaysAssertM(false, s);
				ret = -1;
			}
			catch (const TextInput::WrongTokenType& t) {
				alwaysAssertM(false, t.message);
				ret = -1;
			}
			catch (const TextInput::WrongSymbol& t) {
				alwaysAssertM(false, t.message);
				ret = -1;
			}
			catch (const LightweightConduit::PacketSizeException& e) {
				alwaysAssertM(false, e.message);
				ret = -1;
			}
			catch (const ParseError& e) {
				alwaysAssertM(false, e.formatFileInfo() + e.message);
				ret = -1;
			}
			catch (const FileNotFound& e) {
				alwaysAssertM(false, e.message);
				ret = -1;
			}
			catch (const std::exception& e) {
				alwaysAssertM(false, e.what());
				ret = -1;
			}
		}
		else {
			onRun();
			ret = m_exitCode;
		}

		return ret;
	}

	// run is the next thing after the constructor finishes
	void RApp::onRun() {
		if (window()->requiresMainLoop()) { // this should never be free

			// The window push/pop will take care of
			// calling beginRun/oneFrame/endRun for us.
			window()->pushLoopBody(this);

		}
		else {
			beginRun();

			debugAssertGLOk();

			// now that the scene is set up, we can register all the entities
			if (scene()) {
				Array<shared_ptr<Entity>>* entities = new Array<shared_ptr<Entity>>();
				scene()->getEntityArray(*entities);
				network_node->trackEntities(entities);
			}

			// initialize the connection and wait for the ready
			network_node->init_connection(Constants::ROUTER_ADDR);

			if (network_node->isTypeOf(NodeType::CLIENT)) {
				// Main loop
				do {
					oneFrame();
				} while (!m_endProgram);
			}
			else {
				
				Remote* remote = (Remote*) network_node;
				// set the clipping
				//renderDevice->setClipping(remote->getClip());
				m_finalFrameBuffer = FramebufferDist::create(TextureDist::createEmpty("RApp::m_finalFramebuffer[0]", renderDevice->width(), renderDevice->height(), ImageFormat::RGB8(), Texture::DIM_2D));

				// Busy wait for a message and let receive trigger a render
				do {
					remote->receive();
				} while (!m_endProgram);
			}

			endRun();
		}
	}

	// Similar to oneFrame, this method will call onPose nad onGraphics but will not listen for any
	// user input or do any logic or simulation. Only called by a remote node when it receives network updates
	void RApp::oneFrameAdHoc() {

		// Pose
		BEGIN_PROFILER_EVENT("Pose");
		m_poseWatch.tick(); {
			m_posed3D.fastClear();
			m_posed2D.fastClear();
			onPose(m_posed3D, m_posed2D);

			// The debug camera is not in the scene, so we have
			// to explicitly pose it. This actually does nothing, but
			// it allows us to trigger the TAA code.
			m_debugCamera->onPose(m_posed3D);
		} m_poseWatch.tock();
		END_PROFILER_EVENT();

		// Graphics
		debugAssertGLOk();
		if ((submitToDisplayMode() == SubmitToDisplayMode::BALANCE) && (!renderDevice->swapBuffersAutomatically())) {
			swapBuffers();
		}

		BEGIN_PROFILER_EVENT("Graphics");
		renderDevice->beginFrame();
		// m_widgetManager->onBeforeGraphics();
		m_graphicsWatch.tick(); {
			debugAssertGLOk();
			renderDevice->pushState(); {
				debugAssertGLOk();
				onGraphics(renderDevice, m_posed3D, m_posed2D);
			} renderDevice->popState();
		}  m_graphicsWatch.tock();
		renderDevice->endFrame();
		if ((submitToDisplayMode() == SubmitToDisplayMode::MINIMIZE_LATENCY) && (!renderDevice->swapBuffersAutomatically())) {
			swapBuffers();
		}
		END_PROFILER_EVENT();

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

				SimTime sdt = simStepDuration();
				if (sdt == MATCH_REAL_TIME_TARGET) {
					sdt = realTimeTargetDuration();
				}
				else if (sdt == REAL_TIME) {
					sdt = float(timeStep);
				}
				sdt *= simulationTimeScale();

				SimTime idt = realTimeTargetDuration();

				onBeforeSimulation(rdt, sdt, idt);
				onSimulation(rdt, sdt, idt);
				onAfterSimulation(rdt, sdt, idt);

				// no way to overwrite the base class private fields
				// m_previousSimTimeStep = float(sdt);
				// m_previousRealTimeStep = float(rdt);
				setRealTime(realTime() + rdt);
				setSimTime(simTime() + sdt);
			}
			m_simulationWatch.tock();
			//END_PROFILER_EVENT();
		}

		Client* client = (Client*) network_node;
		// after the simulation period, we will wait until our sands are run
		//RealTime deadline = someTimeStep + 100; // TODO: calculate something here with the given framerate and maybe borrow time from m_renderPeriod

		// send the update
		bool update_sent = client->sendUpdate();
		bool frame_arrived = false;

		if(update_sent)
			while (!frame_arrived) frame_arrived = client->checkNetwork();

		// if there was no update sent, just draw the previous frame
		if (frame_arrived || !update_sent) {
			// display network frame by writing net buffer into native window buffer
			renderDevice->push2D(); {
				Draw::rect2D(finalFrameBuffer()->texture(0)->rect2DBounds(), renderDevice, Color3::white(), finalFrameBuffer()->texture(0));
			} renderDevice->pop2D();
			
			//if (!renderDevice->swapBuffersAutomatically()) {
				swapBuffers();
			//}
			
		}else{

			// Pose
			BEGIN_PROFILER_EVENT("Pose");
			m_poseWatch.tick(); {
				m_posed3D.fastClear();
				m_posed2D.fastClear();
				onPose(m_posed3D, m_posed2D);

				// The debug camera is not in the scene, so we have
				// to explicitly pose it. This actually does nothing, but
				// it allows us to trigger the TAA code.
				m_debugCamera->onPose(m_posed3D);
			} m_poseWatch.tock();
			END_PROFILER_EVENT();

			// Wait
			// Note: we might end up spending all of our time inside of
			// RenderDevice::beginFrame.  Waiting here isn't double waiting,
			// though, because while we're sleeping the CPU the GPU is working
			// to catch up.
			BEGIN_PROFILER_EVENT("Wait");
			m_waitWatch.tick(); {
				RealTime nowAfterLoop = System::time();

				// Compute accumulated time
				RealTime cumulativeTime = nowAfterLoop - r_lastWaitTime;

				//debugAssert(m_wallClockTargetDuration < finf());
				// Perform wait for actual time needed
				RealTime duration = realTimeTargetDuration();
				if (!window()->hasFocus() && lowerFrameRateInBackground()) {
					// Lower frame rate
					duration = 1.0 / BACKGROUND_FRAME_RATE;
				}
				RealTime desiredWaitTime = std::max(0.0, duration - cumulativeTime);
				onWait(std::max(0.0, desiredWaitTime - m_lastFrameOverWait) * 0.97);

				// Update wait timers
				r_lastWaitTime = System::time();
				RealTime actualWaitTime = r_lastWaitTime - nowAfterLoop;

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
			END_PROFILER_EVENT();

			// Graphics
			debugAssertGLOk();
			if ((submitToDisplayMode() == SubmitToDisplayMode::BALANCE) && (!renderDevice->swapBuffersAutomatically())) {
				swapBuffers();
			}

			if (notNull(m_gazeTracker)) {
				BEGIN_PROFILER_EVENT("Gaze Tracker");
				sampleGazeTrackerData();
				END_PROFILER_EVENT();
			}

			BEGIN_PROFILER_EVENT("Graphics");
			renderDevice->beginFrame();
			m_widgetManager->onBeforeGraphics();
			m_graphicsWatch.tick(); {
				debugAssertGLOk();
				renderDevice->pushState(); {
					debugAssertGLOk();
					onGraphics(renderDevice, m_posed3D, m_posed2D);
				} renderDevice->popState();
			}  m_graphicsWatch.tock();
			renderDevice->endFrame();
			if ((submitToDisplayMode() == SubmitToDisplayMode::MINIMIZE_LATENCY) && (!renderDevice->swapBuffersAutomatically())) {
				swapBuffers();
			}
			END_PROFILER_EVENT();
		}
		

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
			debugAssert(notNull(activeCamera()));
			rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
			onGraphics3D(rd, posed3D);
		} rd->popState();

		if (notNull(screenCapture())) {
			screenCapture()->onAfterGraphics3D(rd);
		}

		rd->push2D(); {
			onGraphics2D(rd, posed2D);
		} rd->pop2D();

		if (notNull(screenCapture())) {
			screenCapture()->onAfterGraphics2D(rd);
		}
	}

	void RApp::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {

		//Gate to only bind frame buffer if it is a remote node
		if (network_node->isTypeOf(NodeType::REMOTE)) {
			rd->pushState(m_finalFrameBuffer);
			rd->setClip2D(((Remote*)network_node)->getClip());
		}

	    if (!scene()) {
	        if ((submitToDisplayMode() == SubmitToDisplayMode::MAXIMIZE_THROUGHPUT) && (! rd->swapBuffersAutomatically())) {
	            swapBuffers();
	        }
	        rd->clear();
			rd->pushState(); {
	            rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
	            drawDebugShapes();
	        } rd->popState();
	        return;
	    }

	    BEGIN_PROFILER_EVENT("GApp::onGraphics3D");
	    GBuffer::Specification gbufferSpec = m_gbufferSpecification;
	    extendGBufferSpecification(gbufferSpec);
	    m_gbuffer->setSpecification(gbufferSpec);

	    const Vector2int32 framebufferSize = m_settings.hdrFramebuffer.hdrFramebufferSizeFromDeviceSize(Vector2int32(m_deviceFramebuffer->vector2Bounds()));
	    m_framebuffer->resize(framebufferSize);
	    m_gbuffer->resize(framebufferSize);
	    m_gbuffer->prepare(rd, activeCamera(), 0, -(float)previousSimTimeStep(), m_settings.hdrFramebuffer.depthGuardBandThickness, m_settings.hdrFramebuffer.colorGuardBandThickness);

	    m_renderer->render(rd, activeCamera(), m_framebuffer, scene()->lightingEnvironment().ambientOcclusionSettings.enabled ? m_depthPeelFramebuffer : nullptr, 
	        scene()->lightingEnvironment(), m_gbuffer, allSurfaces);

	    // Debug visualizations and post-process effects
		rd->pushState(m_framebuffer); {

	        // Call to make the App show the output of debugDraw(...)
	        rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
	        drawDebugShapes();
	        const shared_ptr<Entity>& selectedEntity = (notNull(developerWindow) && notNull(developerWindow->sceneEditorWindow)) ? developerWindow->sceneEditorWindow->selectedEntity() : nullptr;
	        scene()->visualize(rd, selectedEntity, allSurfaces, sceneVisualizationSettings(), activeCamera());

	        onPostProcessHDR3DEffects(rd);
	    } rd->popState();

	    // We're about to render to the actual back buffer, so swap the buffers now.
	    // This call also allows the screenshot and video recording to capture the
	    // previous frame just before it is displayed.
	    if (submitToDisplayMode() == SubmitToDisplayMode::MAXIMIZE_THROUGHPUT) {
	        swapBuffers();
	    }

	    // Clear the entire screen (needed even though we'll render over it, since
	    // AFR uses clear() to detect that the buffer is not re-used.)
	    BEGIN_PROFILER_EVENT("RenderDevice::clear");
	    rd->clear();
	    END_PROFILER_EVENT();

	    // Perform gamma correction, bloom, and SSAA, and write to the native window frame buffer
	    m_film->exposeAndRender(rd, activeCamera()->filmSettings(), m_framebuffer->texture(0), settings().hdrFramebuffer.colorGuardBandThickness.x + settings().hdrFramebuffer.depthGuardBandThickness.x, settings().hdrFramebuffer.depthGuardBandThickness.x, 
	        Texture::opaqueBlackIfNull(notNull(m_gbuffer) ? m_gbuffer->texture(GBuffer::Field::SS_POSITION_CHANGE) : nullptr),
	        activeCamera()->jitterMotion());
	    END_PROFILER_EVENT();
		
		//End gate
		if (network_node->isTypeOf(NodeType::REMOTE))
			rd->popState();

	}

	void RApp::onCleanup(){
	}

	void RApp::endProgram(){
		network_node->disconnect();
	}

}
