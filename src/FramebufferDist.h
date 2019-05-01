#pragma once
#include "G3D-gfx/Framebuffer.h"
#include "TextureDist.h"

namespace G3D {

	class FramebufferDist : public Framebuffer {

		class Attachment : public ReferenceCountedObject {
		public:
			friend class FramebufferDist;

			enum Type {
				TEXTURE,
				/** DUMMY attachment is used as a proxy for
					framebuffer parameters (resolution, num layers,
					etc.) when using no-attachment FBO.
				*/
				DUMMY
			};

		protected:

			Color4                      m_clearValue;

			Type                        m_type;

			AttachmentPoint             m_point;

			shared_ptr<TextureDist>         m_texture;

			/** If texture is a CubeFace::MAP, this is the face that
				is attached. */
			CubeFace                    m_cubeFace;

			/** Mip level being rendered to */
			int                         m_mipLevel;

			/**
				Individual layer to be bound. If -1, the texture is bound normally,
				if >= 0, it is bound using glFramebufferTextureLayer
			*/
			int                         m_layer;

			/** These parameters are used only for DUMMY attachment, which
				is used when the framebuffer is in no-attachment mode.
				Dummy attahcment do not have any texture
				associated, and thus have to keep parameters here. */
			int                         m_width;
			int                         m_height;
			int                         m_numLayers;
			int                         m_numSamples;
			bool                        m_fixedSamplesLocation;

			Attachment(AttachmentPoint ap, const shared_ptr<TextureDist>& r, CubeFace c, int miplevel, int layer);

			static shared_ptr<Attachment> create(AttachmentPoint ap, const shared_ptr<TextureDist>& r, CubeFace c, int miplevel, int layer) {
				return createShared<Attachment>(ap, r, c, miplevel, layer);
			}

			/** Dummy attachment */
			Attachment(AttachmentPoint ap, int width, int height, int numLayers, int numSamples, bool fixedSamplesLocation);

			/** Assumes the point is correct */
			bool equals(const shared_ptr<TextureDist>& t, CubeFace f, int miplevel, int layer) const;

			bool equals(const shared_ptr<Attachment>& other) const;

			/** Called from sync() to actually force this to be attached
				at the OpenGL level.  Assumes the framebuffer is already
				bound.*/
			void attach() const;

			/** Called from sync() to actually force this to be detached
				at the OpenGL level.  Assumes the framebuffer is already
				bound.*/
			void detach() const;

		public:

			inline Type type() const {
				return m_type;
			}

			inline AttachmentPoint point() const {
				return m_point;
			}

			inline const shared_ptr<TextureDist>& texture() const {
				return m_texture;
			}

			inline CubeFace cubeFace() const {
				return m_cubeFace;
			}

			inline int mipLevel() const {
				return m_mipLevel;
			}

			/** Will be -1 if no layer selected */
			inline int layer() const {
				return m_layer;
			}

			const ImageFormat* format() const;

			Vector2 vector2Bounds() const;

			int width() const;
			int height() const;

			void resize(int w, int h);
		};

	protected:

		Array< shared_ptr<Attachment> > m_desired;

		FramebufferDist(const String& name, GLuint framebufferID) : Framebuffer(name, framebufferID) { }

	public:
		
		
		static shared_ptr<FramebufferDist> create(const String& _name) {
			GLuint _framebufferID;

			// Generate Framebuffer
			glGenFramebuffers(1, &_framebufferID);

			return shared_ptr<FramebufferDist>(new FramebufferDist(_name, _framebufferID));
		}

		static shared_ptr<FramebufferDist> create(const shared_ptr<TextureDist>& t0, const shared_ptr<TextureDist>& t1 = nullptr, const shared_ptr<TextureDist>& t2 = nullptr, const shared_ptr<TextureDist>& t3 = nullptr) {
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

		shared_ptr<FramebufferDist::Attachment> get(AttachmentPoint ap) const {
			alwaysAssertM(m_framebufferID != GL_ZERO, "Cannot get attachments from a hardware framebuffer");

			bool found = false;
			int i = find(ap, found);
			if (!found) {
				return shared_ptr<Attachment>();
			}
			else {
				return m_desired[i];
			}
		}

		/** Shorthand for getting the texture for attachment point x */
		const shared_ptr<TextureDist>& texture(AttachmentPoint x) {
			const shared_ptr<Attachment>& a = get(x);
			if (notNull(a)) {
				return a->texture();
			}
			else {
				static const shared_ptr<TextureDist> undefined;
				return undefined;
			}
		}

		/** Shorthand for getting the texture for color attachment point x*/
		const shared_ptr<TextureDist>& texture(const uint8 x) {
			debugAssertM(x < 16, format("Invalid attachment index: %d", x));
			return texture(AttachmentPoint(x + COLOR0));
		}
		
	};
}