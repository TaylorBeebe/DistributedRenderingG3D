#include "RenderDeviceDist.h"

void RenderDeviceDist::pushState(const shared_ptr<Framebuffer>& fb)
{

	//debugAssert(m_state.drawFramebuffer && m_state.readFramebuffer);
	RenderDevice::pushState();

	if (fb) {
		setFramebuffer(fb);

		// When we change framebuffers, we almost certainly don't want to use the old clipping region
		//setClip2D(Rect2D::inf());
		setClip2D(Rect2D::xyxy(width() / 2, height() / 2, width(), height()));
		setViewport(fb->rect2DBounds());
	}
	//debugAssert(m_state.drawFramebuffer && m_state.readFramebuffer);

}
