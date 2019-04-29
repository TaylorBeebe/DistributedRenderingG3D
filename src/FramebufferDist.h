#pragma once
#include "G3D-gfx/Framebuffer.h"
#include "TextureDist.h"

namespace G3D {

	class FramebufferDist : public Framebuffer {

	public:

		const shared_ptr<TextureDist>& texture(const uint8 x) {
			return texture(AttachmentPoint(x + COLOR0));
		}


	};
}