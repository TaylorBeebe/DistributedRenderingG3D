#pragma once
#include "G3D-gfx/Framebuffer.h"
#include "TextureDist.h"

namespace G3D {

	class FramebufferDist : public Framebuffer {

	protected:

		FramebufferDist(const String& name, GLuint framebufferID) : Framebuffer(name, framebufferID) { }

	public:
		
		
		static shared_ptr<FramebufferDist> create(const String& _name) {
			GLuint _framebufferID;

			// Generate Framebuffer
			glGenFramebuffers(1, &_framebufferID);

			return shared_ptr<FramebufferDist>(new FramebufferDist(_name, _framebufferID));
		}

		static shared_ptr<FramebufferDist> create(const shared_ptr<Texture>& t0, const shared_ptr<Texture>& t1 = nullptr, const shared_ptr<Texture>& t2 = nullptr, const shared_ptr<Texture>& t3 = nullptr) {
			shared_ptr<FramebufferDist> f = create(t0->name() + " framebuffer");

			if (t0) {
				if (t0->format()->depthBits > 0) {
					if (t0->format()->stencilBits > 0) {
						f->set(DEPTH_AND_STENCIL, t0);
					}
					else {
						f->set(DEPTH, t0);
					}
				}
				else {
					f->set(COLOR0, t0);
				}
			}

			if (t1) {
				if (t1->format()->depthBits > 0) {
					if (t1->format()->stencilBits > 0) {
						f->set(DEPTH_AND_STENCIL, t1);
					}
					else {
						f->set(DEPTH, t1);
					}
				}
				else {
					f->set(COLOR1, t1);
				}
			}

			if (t2) {
				if (t2->format()->depthBits > 0) {
					if (t2->format()->stencilBits > 0) {
						f->set(DEPTH_AND_STENCIL, t2);
					}
					else {
						f->set(DEPTH, t2);
					}
				}
				else {
					f->set(COLOR2, t2);
				}
			}

			if (t3) {
				if (t3->format()->depthBits > 0) {
					if (t3->format()->stencilBits > 0) {
						f->set(DEPTH_AND_STENCIL, t3);
					}
					else {
						f->set(DEPTH, t3);
					}
				}
				else {
					f->set(COLOR3, t3);
				}
			}

			return f;
		}

		void setTexture0(const shared_ptr<Texture>& t0) {

			this->set(COLOR0, t0);
		
		}
		
	};
}