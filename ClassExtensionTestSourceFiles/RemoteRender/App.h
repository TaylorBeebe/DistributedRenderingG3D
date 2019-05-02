/**
  \file App.h
 */
#ifndef App_h
#define App_h
#include <G3D/G3D.h>
#include <FramebufferDist.h>

#define WEB_PORT (8080)

shared_ptr<Texture> qrEncodeHTTPAddress(const NetAddress& addr);

/** 
  Simple example of sending G3D events from a web browser and injecting them
  into the GApp event system and sending images in real-time to a web browser.

  Connects G3D to codeheart.js.

  Connect to the displayed URL from any browser or use the displayed 
  QR code to automatically connect from a mobile device. */
class App : public GApp {
protected:    
    bool                    m_showWireframe;

    shared_ptr<WebServer>   m_webServer;

    shared_ptr<GFont>       m_font;
    String                  m_addressString;

	/** QR code for letting clients automatically connect */
	shared_ptr<Texture>     m_qrTexture;
	shared_ptr<Texture>     m_background;

    /** The image sent across the network */
    shared_ptr<FramebufferDist> m_finalFramebuffer;

    /** Called from onInit */
    void makeGUI();

    void startWebServer();
    void stopWebServer();
    void handleRemoteEvents();

public:
    
    App(const GApp::Settings& settings = GApp::Settings());

    virtual void onInit() override;

    virtual void onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surface3D) override;
    virtual void onGraphics2D(RenderDevice* rd, Array<shared_ptr<Surface2D> >& surface2D) override;

    virtual void onNetwork() override;
    virtual bool onEvent(const GEvent& e) override;
    virtual void onCleanup() override;
    
    /** Sets m_endProgram to true. */
    virtual void endProgram();
};


class MySocket : public WebServer::WebSocket {
protected:
    MySocket(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) : WebSocket(server, connection, clientAddress) {}
public:
    static shared_ptr<WebSocket> create(WebServer* server, mg_connection* connection, const NetAddress& clientAddress) {
        return createShared<MySocket>(server, connection, clientAddress);
    }

    bool onConnect() override;

    void onReady() override;

    bool onData(Opcode opcode, char* data, size_t data_len) override;

};

#endif
