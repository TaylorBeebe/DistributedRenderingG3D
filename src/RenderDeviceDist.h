#pragma once
#include <G3D/G3D.h>

class RenderDeviceDist : public RenderDevice {

protected:

	int screenShareUpper = 0;
	int screenShareLower = height();

public:

	void setScreenShare(int lower, int upper) {
		
		screenShareLower = lower;
		screenShareUpper = upper;

	}

	void pushState(const shared_ptr<Framebuffer>& fb) {
		
		RenderDevice::pushState();

		if (fb) {
			setFramebuffer(fb);
			setClip2D(Rect2D::xyxy(0, screenShareLower, width(),screenShareUpper));
			setViewport(fb->rect2DBounds());
		}
	}
};