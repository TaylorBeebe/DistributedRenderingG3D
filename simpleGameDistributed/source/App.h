#ifndef App_h
#define App_h

#include <G3D/G3D.h>
#include "../src/DistributedRenderer.h"

class PhysicsScene;

using namespace DistributedRenderer;

class App : public RApp {
protected:

    bool                        m_firstPersonMode;

    String                      m_playerName;

    shared_ptr<PhysicsScene>    m_scene;    
    
	shared_ptr<FramebufferDist> m_finalFramebuffer;

    /** Called from onInit */
    void makeGUI();

public:
    
    App(const GApp::Settings& settings, NodeType node_type);

    virtual void onInit() override;
    virtual void onAI() override;
    virtual void onNetwork() override;
    virtual void onSimulation(RealTime rdt, SimTime sdt, SimTime idt) override;
    virtual void onPose(Array<shared_ptr<Surface> >& posed3D, Array<shared_ptr<Surface2D> >& posed2D) override;
	
	virtual void onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surface3D) override;

    virtual bool onEvent(const GEvent& e) override;
    virtual void onUserInput(UserInput* ui) override;
};

#endif
