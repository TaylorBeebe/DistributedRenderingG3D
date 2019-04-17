#pragma once
#include <G3D/G3D.h>

class RenderDeviceDist : public RenderDevice {

	void pushState(const shared_ptr<Framebuffer>& fb);

};