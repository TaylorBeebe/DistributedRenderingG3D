#pragma once
#include <G3D/G3D.h>

class TextureDist : public Texture {

public:

	static GLenum dimensionToTarget(Texture::Dimension d, int numSamples) {
		switch (d) {
		case Texture::DIM_CUBE_MAP:
			return GL_TEXTURE_CUBE_MAP;

		case Texture::DIM_CUBE_MAP_ARRAY:
			return GL_TEXTURE_CUBE_MAP_ARRAY;

		case Texture::DIM_2D:
			if (numSamples < 2) {
				return GL_TEXTURE_2D;
			}
			else {
				return GL_TEXTURE_2D_MULTISAMPLE;
			}
		case Texture::DIM_2D_ARRAY:
			return GL_TEXTURE_2D_ARRAY;

		case Texture::DIM_2D_RECT:
			return GL_TEXTURE_RECTANGLE_EXT;

		case Texture::DIM_3D:
			return GL_TEXTURE_3D;

		default:
			debugAssert(false);
			return 0;//GL_TEXTURE_2D;
		}
	}

	static void createTexture
	(GLenum          target,
		const uint8*    rawBytes,
		GLenum          bytesActualFormat,
		GLenum          bytesFormat,
		int             m_width,
		int             m_height,
		int             depth,
		GLenum          ImageFormat,
		int             bytesPerPixel,
		int             mipLevel,
		bool            compressed,
		GLenum          dataType,
		int             numSamples,
		const Texture::Encoding& encoding) {

		uint8* bytes = const_cast<uint8*>(rawBytes);

		// If true, we're supposed to free the byte array at the end of
		// the function.
		bool   freeBytes = false;

		int maxSize = GLCaps::maxTextureSize();

		switch (target) {
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			maxSize = GLCaps::maxCubeMapSize();
			// Fall through

		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_RECTANGLE:

			// Note code falling through from above

			if (compressed) {
				//debugAssertM((target != GL_TEXTURE_RECTANGLE),
				//	"Compressed textures must be DIM_2D or DIM_2D.");

				glCompressedTexImage2DARB
				(target, mipLevel, bytesActualFormat, m_width,
					m_height, 0, (bytesPerPixel * ((m_width + 3) / 4) * ((m_height + 3) / 4)),
					rawBytes);

			}
			else {

				if (notNull(bytes)) {
					//debugAssert(isValidPointer(bytes));
					//debugAssertM(isValidPointer(bytes + (m_width * m_height - 1) * bytesPerPixel),
						"Byte array in Texture creation was too small");
				}

				// 2D texture, level of detail 0 (normal), internal
				// format, x size from image, y size from image, border 0
				// (normal), rgb color data, unsigned byte data, and
				// finally the data itself.
				glPixelStorei(GL_PACK_ALIGNMENT, 1);

				if (target == GL_TEXTURE_2D_MULTISAMPLE) {
					glTexImage2DMultisample(target, numSamples, ImageFormat, m_width, m_height, false);
				}
				else {
					//debugAssertGLOk();
					glTexImage2D(target, mipLevel, ImageFormat, m_width, m_height, 0, bytesFormat, dataType, bytes);
					//debugAssertGLOk();
				}
			}
			break;

		case GL_TEXTURE_3D:
		case GL_TEXTURE_2D_ARRAY:
			//debugAssert(isNull(bytes) || isValidPointer(bytes));
			glTexImage3D(target, mipLevel, ImageFormat, m_width, m_height, depth, 0, bytesFormat, dataType, bytes);
			break;
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			//debugAssert(isNull(bytes) || isValidPointer(bytes));
			glTexImage3D(target, mipLevel, ImageFormat, m_width, m_height,
				depth * 6, 0, bytesFormat, dataType, bytes);
			break;
		default:
			//debugAssertM(false, "Fell through switch");
		}

		if (freeBytes) {
			// Texture was resized; free the temporary.
			delete[] bytes;
		}
	}

	shared_ptr<TextureDist> fromGLTexture
	(const String&           name,
		GLuint                  textureID,
		Encoding                encoding,
		AlphaFilter             alphaFilter,
		Dimension               dimension,
		bool                    destroyGLTextureInDestructor,
		int                     numSamples,
		int                     width,
		int                     height,
		int                     depth,
		bool                    hasMIPMaps) {

		debugAssert(encoding.format);

		// Detect dimensions
		const GLenum target = dimensionToTarget(dimension, numSamples);
		debugAssertGLOk();

		// For cube map, we can't read "cube map" but must choose a face
		const GLenum readbackTarget = (dimension == DIM_CUBE_MAP) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : target;

		if (width == -1 || height == -1 || depth == -1) {
			glBindTexture(target, textureID);
			glGetTexLevelParameteriv(readbackTarget, 0, GL_TEXTURE_WIDTH, &width);
			glGetTexLevelParameteriv(readbackTarget, 0, GL_TEXTURE_HEIGHT, &height);

			if ((readbackTarget == GL_TEXTURE_3D) || (readbackTarget == GL_TEXTURE_2D_ARRAY)) {
				glGetTexLevelParameteriv(readbackTarget, 0, GL_TEXTURE_DEPTH, &depth);
			}
			glBindTexture(target, GL_NONE);
		}

		const shared_ptr<Texture> t = createShared<Texture>(name, width, height, depth, dimension, encoding, numSamples, false);
		s_allTextures.set((uintptr_t)t.get(), t);
		t->m_conservativelyHasNonUnitAlpha = (encoding.format->alphaBits > 0) ||
			(encoding.readMultiplyFirst.a + encoding.readAddSecond.a < 1.0f);
		t->m_conservativelyHasUnitAlpha = ((encoding.format->alphaBits == 0) &&
			(encoding.readMultiplyFirst.a + encoding.readAddSecond.a >= 1.0f)) ||
			(encoding.readAddSecond.a >= 1.0f);
		t->m_textureID = textureID;
		t->m_detectedHint = alphaFilter;
		t->m_opaque = (encoding.readMultiplyFirst.a >= 1.0f) && (encoding.format->alphaBits == 0);
		t->m_encoding = encoding;
		t->m_hasMipMaps = hasMIPMaps;
		t->m_appearsInTextureBrowserWindow = true;
		t->m_destroyGLTextureInDestructor = destroyGLTextureInDestructor;

		t->m_loadingInfo = new LoadingInfo(LoadingInfo::SET_SAMPLER_PARAMETERS);
		t->completeGPULoading();

		return t;
	}


	static void glStatePush() {
		glActiveTexture(GL_TEXTURE0);
	}

	static void glStatePop() {
	}

	shared_ptr<TextureDist> createEmpty
	(const String&                    name,
		int                              width,
		int                              height,
		const Encoding&                  encoding,
		Dimension                        dimension,
		bool                             allocateMIPMaps,
		int                              depth,
		int                              numSamples) {

		//debugAssertGLOk();
		//debugAssertM(encoding.format, "encoding.format may not be ImageFormat::AUTO()");

		if ((dimension != DIM_3D) && (dimension != DIM_2D_ARRAY) && (dimension != DIM_CUBE_MAP_ARRAY)) {
			//debugAssertM(depth == 1, "Depth must be 1 for DIM_2D textures");
		}

		//debugAssert(notNull(encoding.format));

		// Check for at least one miplevel on the incoming data
		int maxRes = std::max(width, std::max(height, depth));
		int numMipMaps = allocateMIPMaps ? int(log2(float(maxRes))) + 1 : 1;
		//debugAssert(numMipMaps > 0);

		// Create the texture
		GLuint textureID = newGLTextureID();
		GLenum target = dimensionToTarget(dimension, numSamples);

		//debugAssertM(GLCaps::supportsTexture(encoding.format), "Unsupported texture format.");

		int mipWidth = width;
		int mipHeight = height;
		int mipDepth = depth;
		Color4 minval = Color4::nan();
		Color4 meanval = Color4::nan();
		Color4 maxval = Color4::nan();
		AlphaFilter alphaFilter = AlphaFilter::DETECT;

		glStatePush(); {

			glBindTexture(target, textureID);
			//debugAssertGLOk();

			if (GLCaps::supports_glTexStorage2D() && ((target == GL_TEXTURE_2D) || (target == GL_TEXTURE_CUBE_MAP))) {
				glTexStorage2D(target, numMipMaps, encoding.format->openGLFormat, width, height);
			}
			else {
				for (int mipLevel = 0; mipLevel < numMipMaps; ++mipLevel) {
					int numFaces = (dimension == DIM_CUBE_MAP) ? 6 : 1;

					for (int f = 0; f < numFaces; ++f) {
						if (numFaces == 6) {
							// Choose the appropriate face target
							target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + f;
						}

						//debugAssertGLOk();
						createTexture(target,
							nullptr,
							encoding.format->openGLFormat,
							encoding.format->openGLBaseFormat,
							mipWidth,
							mipHeight,
							mipDepth,
							encoding.format->openGLFormat,
							encoding.format->cpuBitsPerPixel / 8,
							mipLevel,
							encoding.format->compressed,
							encoding.format->openGLDataFormat,
							numSamples,
							encoding);
						//debugAssertGLOk();

#if 0 // Avoid GPU-CPU sync
#               ifndef G3D_DEBUG
						{
							GLenum e = glGetError();
							if (e == GL_OUT_OF_MEMORY) {
								throw String("The texture map was too large (GL_OUT_OF_MEMORY)");
							}
							if (e != GL_NO_ERROR) {
								String errors;
								while (e != GL_NO_ERROR) {
									e = glGetError();
									if (e == GL_OUT_OF_MEMORY) {
										throw String("The texture map was too large (GL_OUT_OF_MEMORY)");
									}
								}
							}
						}
#               endif
#endif
					}

					mipWidth = G3D::max(1, mipWidth / 2);
					mipHeight = G3D::max(1, mipHeight / 2);
					mipDepth = G3D::max(1, mipDepth / 2);
				}
			}

		} glStatePop();

		//debugAssertGLOk();
		const shared_ptr<TextureDist>& t = fromGLTexture(name, textureID, encoding.format, alphaFilter, dimension);
		//debugAssertGLOk();

		t->m_width = width;
		t->m_height = height;
		t->m_depth = depth;
		t->m_min = minval;
		t->m_max = maxval;
		t->m_mean = meanval;
		t->m_hasMipMaps = allocateMIPMaps;

		t->m_encoding = encoding;
		if (encoding.format->depthBits > 0) {
			t->visualization = Visualization::depthBuffer();
		}

		if (allocateMIPMaps) {
			// Some GPU drivers will not allocate the MIP levels until
			// this is called explicitly, which can cause framebuffer
			// calls to fail
			t->generateMipMaps();
		}

		//debugAssertGLOk();
		return t;
	}

	/*
	void toPixelTransferBuffer(shared_ptr<GLPixelTransferBuffer>& buffer, const ImageFormat* outFormat, int mipLevel, CubeFace face) {
		Texture::toPixelTransferBuffer(buffer, outFormat, mipLevel, face);
	}

	shared_ptr<GLPixelTransferBuffer> toPixelTransferBuffer(const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X) {
		return Texture::toPixelTransferBuffer(outFormat, mipLevel, face);
	}

	*/
	shared_ptr<Image> toImage5(const ImageFormat* outFormat, int w, int h, int mipLevel, CubeFace face) const {
		return Image::fromPixelTransferBuffer(Texture::toPixelTransferBuffer(outFormat, mipLevel, face));
	}
	
};