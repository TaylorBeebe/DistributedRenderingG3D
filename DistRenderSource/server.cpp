/** \file App.cpp

*/
#include "Server.h"
#include <time.h>
#include <mutex>

// Tells C++ to invoke command-line main() function even on OS X and Win32.
G3D_START_AT_MAIN();

/** Events coming in from the remote machine */
static ThreadsafeQueue<GEvent>      remoteEventQueue;

/** Set to 1 when the client requests a full-screen image and back to 0 after the frame is sent. */
static std::atomic_int              clientWantsImage(0);

/** Socket URI used to link the connections*/
static const String                 socketUri = "/websocket";

int main(int argc, const char* argv[]) {
	initGLG3D();

	GApp::Settings settings(argc, argv);

	settings.window.width = 640;    settings.window.height = 400;

	alwaysAssertM(FileSystem::exists("www"), "Not running from the contents of the data-files directory");

	return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings), m_webServer(WebServer::create()) {
}


void App::onInit() {
	GApp::onInit();
	renderDevice->setSwapBuffersAutomatically(true);

	showRenderingStats = false;
	m_showWireframe = false;

	// May be using a web browser on the same machine in the foreground
	setLowerFrameRateInBackground(false);
	//30fps
	setFrameDuration(1.0f / 30);

	makeGUI();

	developerWindow->videoRecordDialog->setCaptureGui(false);
	developerWindow->setVisible(false);
	developerWindow->sceneEditorWindow->setVisible(false);
	developerWindow->cameraControlWindow->setVisible(false);

	developerWindow->cameraControlWindow->moveTo(Point2(developerWindow->cameraControlWindow->rect().x0(), 0));
	m_finalFramebuffer = Framebuffer::create(Texture::createEmpty("App::m_finalFramebuffer[0]", renderDevice->width(), renderDevice->height(), ImageFormat::RGB8(), Texture::DIM_2D));

	loadScene("G3D Simple Cornell Box");

	setActiveCamera(m_debugCamera);

	startWebServer();

	m_font = GFont::fromFile(System::findDataFile("arial.fnt"));
	const NetAddress serverAddress(NetAddress::localHostname(), WEB_PORT);
	m_addressString = serverAddress.toString();
	debugPrintf("Server Address: %s\n", serverAddress.toString().c_str());
}


void printRequest(const mg_request_info* request_info) {

}

void App::startWebServer() {
	alwaysAssertM(notNull(m_webServer), "server is null");

	// start the server first then add handlers, the order matters
	// Start the web server.
	m_webServer->start();

	// Register websocket handlers
	m_webServer->registerWebSocketHandler(socketUri, &MySocket::create);

}


void App::stopWebServer() {
	alwaysAssertM(notNull(m_webServer), "server is null");
	m_webServer->stop();
}


void App::makeGUI() {
	debugWindow->setVisible(false);
	developerWindow->videoRecordDialog->setEnabled(true);

	debugWindow->pack();
	debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
}


void App::onNetwork() {
	handleRemoteEvents();
}


void App::handleRemoteEvents() {
	userInput->beginEvents();
	GEvent event;

	// Inject these events as if they occured locally
	while (remoteEventQueue.popFront(event)) {
		if (!WidgetManager::onEvent(event, m_widgetManager)) {
			if (!onEvent(event)) {
				userInput->processEvent(event);
			}
		}
	}

	userInput->endEvents();
}


bool App::onEvent(const GEvent& event) {
	// Handle super-class events
	if (GApp::onEvent(event)) { return true; }

	if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == 'p')) {
		// Send a ping message to the clients. Specific to this application
		// and only used for testing.
		const String msg = "{\"type\": 0, \"value\": \"how are you?\"}";

		Array<shared_ptr<WebServer::WebSocket>> array;  m_webServer->getWebSocketArray(socketUri, array);
		for (const shared_ptr<WebServer::WebSocket>& socket : array) {
			socket->send(msg);
		}
		return true;
	}

	return false;
}


static void sendImage(const shared_ptr<Image>& image, const Array<shared_ptr<WebServer::WebSocket>>& socketArray, Image::ImageFileFormat ff) {
	static const int IMAGE = 1;
	BinaryOutput bo("<memory>", G3D_BIG_ENDIAN);

	alwaysAssertM((ff == Image::PNG) || (ff == Image::JPEG), "Only PNG and JPEG are supported right now");
	const char* mimeType = (ff == Image::PNG) ? "image/png" : "image/jpeg";
	const String& msg =
		format("{\"type\":%d,\"width\":%d,\"height\":%d,\"mimeType\":\"%s\"}", IMAGE, image->width(), image->height(), mimeType);

	// JSON header length (in network byte order)
	bo.writeInt32((int32)msg.length());

	// JSON header
	bo.writeString(msg, (int32)msg.length());

	// Binary data
	image->serialize(bo, ff);

	// Send to all children
	for (const shared_ptr<WebServer::WebSocket>& socket : socketArray) {
		const size_t bytes = socket->send(bo);
		(void)bytes;
		// debugPrintf("Sent %d bytes\n", (unsigned int)bytes);
	}
}


void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {
	// Perform gamma correction, bloom, and SSAA, and write to the native window frame buffer
	rd->pushState(m_finalFramebuffer); {
		GApp::onGraphics3D(rd, allSurfaces);
	} rd->popState();

	// Copy the final buffer to the server screen
	rd->push2D(); {
		Draw::rect2D(m_finalFramebuffer->texture(0)->rect2DBounds(), rd, Color3::white(), m_finalFramebuffer->texture(0));
	} rd->pop2D();

	if (clientWantsImage.load() != 0) {
		Array<shared_ptr<WebServer::WebSocket>> array;
		m_webServer->getWebSocketArray(socketUri, array);

		// JPEG encoding/decoding takes more time but substantially less bandwidth than PNG
		sendImage(m_finalFramebuffer->texture(0)->toImage(ImageFormat::RGB8()), array, Image::JPEG);
		clientWantsImage = 0;
	}
}


void App::onGraphics2D(RenderDevice* rd, Array<shared_ptr<Surface2D> >& posed2D) {

	if (m_font) {
		Array<shared_ptr<WebServer::WebSocket>> array;  m_webServer->getWebSocketArray(socketUri, array);
		m_font->draw2D(rd, format("%d clients connected:", array.size()), Vector2(400, 10), 18, Color3::white(), Color3::black());
		float y = 40;
		for (const shared_ptr<WebServer::WebSocket>& socket : array) {
			y += m_font->draw2D(rd, socket->clientAddress.toString(), Vector2(400, y), 12, Color3::white(), Color3::black()).y + 5;
		}
	}


	// Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction.
	Surface2D::sortAndRender(rd, posed2D);
}


void App::onCleanup() {
	stopWebServer();
}


void App::endProgram() {
	m_endProgram = true;
}

bool MySocket::onConnect() {
	return true;
}


void MySocket::onReady() {
	// Handshake with a new client
	send("{\"type\": 0, \"value\":\"server ready\"}");
	clientWantsImage = 1;
}


bool MySocket::onData(Opcode opcode, char* data, size_t data_len) {

	// Program currently ignores anything not a TEXT
	if (opcode != TEXT) return true;

	if ((data_len == 6) && (memcmp(data, "\"ping\"", 6) == 0)) {
		// This is our application protocol ping message; ignore it
		return true;
	}

	if ((data_len < 2) || (data[0] != '{')) {
		// Some corrupt message
		debugPrintf("Message makes no sense\n");
		return true;
	}


	try {
		TextInput t(TextInput::FROM_STRING, data, data_len);
		const Any msg(t);

		const int UNKNOWN = 0;
		const int SEND_IMAGE = 1000;
		const int type = msg.get("type", UNKNOWN);

		switch (type) {
		case UNKNOWN:
			debugPrintf("Cannot identify message type\n");
			break;

		case SEND_IMAGE:
			clientWantsImage = 1;
			break;

		case GEventType::KEY_DOWN:
		case GEventType::KEY_UP:
		{
			GEvent event;
			memset(&event, 0, sizeof(event));
			event.type = type;
			const Any& key = msg.get("key", Any());
			const Any& keysym = key.get("keysym", Any());
			event.key.keysym.sym = GKey::Value((int)keysym.get("sym", 0));
			event.key.state = (type == GEventType::KEY_DOWN) ? GButtonState::PRESSED : GButtonState::RELEASED;
			remoteEventQueue.pushBack(event);
		}
		break;

		default:
			debugPrintf("Unrecognized type\n");
			break;
		};

	}
	catch (...) {
		debugPrintf("Message makes no sense\n");
	}

	// Returning zero means stoping websocket conversation.
	return true;
}