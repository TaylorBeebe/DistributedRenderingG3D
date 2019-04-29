#pragma once
#include <G3D/G3D.h>

class RenderDeviceDist : public RenderDevice {

	void pushState(const shared_ptr<Framebuffer>& fb) {
		
		RenderDevice::pushState();

		if (fb) {
			setFramebuffer(fb);
			setClip2D(Rect2D::xyxy(width() / 2, height() / 2, width(), height()));
			setViewport(fb->rect2DBounds());
		}
	}
};