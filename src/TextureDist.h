#pragma once
#include <G3D/G3D.h>

class TextureDist : public Texture {

public:

	TextureDist(const String& name,int width, int height,int depth,Dimension dimension,const Encoding& encoding,int numSamples,bool needsForce) : 
		Texture::Texture(name, width, height, depth, dimension, encoding, numSamples, needsForce) {}


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
			//debugAssert(false);
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

				glCompressedTexImage2DARB
				(target, mipLevel, bytesActualFormat, m_width,
					m_height, 0, (bytesPerPixel * ((m_width + 3) / 4) * ((m_height + 3) / 4)),
					rawBytes);

			}
			else {

				// 2D texture, level of detail 0 (normal), internal
				// format, x size from image, y size from image, border 0
				// (normal), rgb color data, unsigned byte data, and
				// finally the data itself.
				glPixelStorei(GL_PACK_ALIGNMENT, 1);

				if (target == GL_TEXTURE_2D_MULTISAMPLE) {
					glTexImage2DMultisample(target, numSamples, ImageFormat, m_width, m_height, false);
				}
				else {
					glTexImage2D(target, mipLevel, ImageFormat, m_width, m_height, 0, bytesFormat, dataType, bytes);
				}
			}
			break;

		case GL_TEXTURE_3D:
		case GL_TEXTURE_2D_ARRAY:
			glTexImage3D(target, mipLevel, ImageFormat, m_width, m_height, depth, 0, bytesFormat, dataType, bytes);
			break;
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			glTexImage3D(target, mipLevel, ImageFormat, m_width, m_height,
				depth * 6, 0, bytesFormat, dataType, bytes);
			break;
		}

		if (freeBytes) {
			// Texture was resized; free the temporary.
			delete[] bytes;
		}
	}

	static shared_ptr<TextureDist> fromGLTexture
	(const String&                   name,
		GLuint                          textureID,
		Encoding                        encoding,
		AlphaFilter                     alphaFilter,
		Dimension                       dimension = DIM_2D,
		bool                            destroyGLTextureInDestructor = true,
		int                             numSamples = 1,
		int                             width = -1,
		int                             height = -1,
		int                             depth = -1,
		bool                            hasMIPMaps = false) {


		// Detect dimensions
		const GLenum target = dimensionToTarget(dimension, numSamples);

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

		const shared_ptr<TextureDist> t = createShared<TextureDist>(name, width, height, depth, dimension, encoding, numSamples, false);
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

	static shared_ptr<TextureDist> createEmpty (const String& name, int width, int height, const Encoding& encoding = Encoding(ImageFormat::RGBA8()), 
		Dimension dimension = DIM_2D, bool allocateMIPMaps = false, int depth = 1, int numSamples = 1) {

		// Check for at least one miplevel on the incoming data
		int maxRes = std::max(width, std::max(height, depth));
		int numMipMaps = allocateMIPMaps ? int(std::log2(float(maxRes))) + 1 : 1;
		//debugAssert(numMipMaps > 0);

		// Create the texture
		GLuint textureID = newGLTextureID();
		GLenum target = dimensionToTarget(dimension, numSamples);

		int mipWidth = width;
		int mipHeight = height;
		int mipDepth = depth;
		Color4 minval = Color4::nan();
		Color4 meanval = Color4::nan();
		Color4 maxval = Color4::nan();
		AlphaFilter alphaFilter = AlphaFilter::DETECT;

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


		const shared_ptr<TextureDist>& t = fromGLTexture(name, textureID, encoding.format, alphaFilter, dimension);

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

		return t;
	}

	static bool isSRGBFormat(const ImageFormat* format) {
		return format->colorSpace == ImageFormat::COLOR_SPACE_SRGB;
	}

	static GLint getPackAlignment(int bufferStride, GLint& oldPackAlignment, bool& alignmentNeedsToChange) {
		oldPackAlignment = 8; // LCM of all possible alignments
		int alignmentOffset = bufferStride % oldPackAlignment;
		if (alignmentOffset != 0) {
			glGetIntegerv(GL_PACK_ALIGNMENT, &oldPackAlignment); // find actual alignment
			alignmentOffset = bufferStride % oldPackAlignment;
		}
		alignmentNeedsToChange = alignmentOffset != 0;
		GLint newPackAlignment = oldPackAlignment;
		if (alignmentNeedsToChange) {
			if (alignmentOffset == 4) {
				newPackAlignment = 4;
			}
			else if (alignmentOffset % 2 == 0) {
				newPackAlignment = 2;
			}
			else {
				newPackAlignment = 1;
			}
		}
		return newPackAlignment;
	}

	void toPixelTransferBuffer(shared_ptr<GLPixelTransferBuffer>& buffer, const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X, bool runMapHooks = true) const {
	debugAssertGLOk();
	force();
	if (outFormat == ImageFormat::AUTO()) {
		outFormat = format();
	}
	debugAssertGLOk();
	alwaysAssertM(!isSRGBFormat(outFormat) || isSRGBFormat(format()), "glGetTexImage doesn't do sRGB conversion, so we need to first copy an RGB texture to sRGB on the GPU. However, this functionality is broken as of the time of writing this code");

	const bool cpuSRGBConversion = isSRGBFormat(format()) && !isSRGBFormat(outFormat) && (m_dimension == DIM_CUBE_MAP);

	BEGIN_PROFILER_EVENT("G3D::Texture::toPixelTransferBuffer");

	if (outFormat == format()) {
		if (outFormat == ImageFormat::SRGB8()) {
			outFormat = ImageFormat::RGB8();
		}
		else if (outFormat == ImageFormat::SRGBA8()) {
			outFormat = ImageFormat::RGBA8();
		}
	}

	// Need to call before binding in case an external
	// application (CUDA) has this buffer mapped.
	if (runMapHooks) {
		buffer->runMapHooks();
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer->glBufferID()); {
		debugAssertGLOk();

		glBindTexture(openGLTextureTarget(), openGLID()); {
			debugAssertGLOk();
			GLenum target;
			if (isCubeMap()) {
				target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face;
			}
			else {
				// Not a cubemap
				target = openGLTextureTarget();
			}

			bool alignmentNeedsToChange;
			GLint oldPackAlignment;
			const GLint newPackAlignment = getPackAlignment((int)buffer->stride(), oldPackAlignment, alignmentNeedsToChange);

			debugAssertM(!((outFormat == ImageFormat::R32F()) && (m_encoding.format == ImageFormat::DEPTH32F())), "Read back DEPTH32F as DEPTH32F, not R32F");
			if (alignmentNeedsToChange) {
				glPixelStorei(GL_PACK_ALIGNMENT, newPackAlignment);
				debugAssertGLOk();
			}

			BEGIN_PROFILER_EVENT("glGetTexImage");
			debugAssertGLOk();
			glGetTexImage(target, mipLevel, outFormat->openGLBaseFormat, outFormat->openGLDataFormat, 0);
			debugAssertGLOk();
			END_PROFILER_EVENT();
			if (alignmentNeedsToChange) {
				glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
				debugAssertGLOk();
			}

		} glBindTexture(openGLTextureTarget(), GL_NONE);

	} glBindBuffer(GL_PIXEL_PACK_BUFFER, GL_NONE);
	debugAssertGLOk();

	if (cpuSRGBConversion) {
		BEGIN_PROFILER_EVENT("CPU sRGB -> RGB conversion");
		// Fix sRGB
		alwaysAssertM(outFormat == ImageFormat::RGB32F(), "CubeMap sRGB -> RGB conversion only supported for RGB32F format output");
		Color3* ptr = (Color3*)buffer->mapReadWrite();
		runConcurrently(0, int(buffer->size() / sizeof(Color3)), [&](int i) {
			ptr[i] = ptr[i].sRGBToRGB();
		});
		buffer->unmap();
		ptr = nullptr;
		END_PROFILER_EVENT();
	}

	END_PROFILER_EVENT();
}

shared_ptr<GLPixelTransferBuffer> toPixelTransferBuffer(const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X) const {
	force();
	if (outFormat == ImageFormat::AUTO()) {
		outFormat = format();
	}
	debugAssertGLOk();
	alwaysAssertM(!isSRGBFormat(outFormat) || isSRGBFormat(format()), "glGetTexImage doesn't do sRGB conversion, so we need to first copy an RGB texture to sRGB on the GPU. However, this functionality is broken as of the time of writing this code");

	const bool cpuSRGBConversion = isSRGBFormat(format()) && !isSRGBFormat(outFormat) && (m_dimension == DIM_CUBE_MAP);

	if (isSRGBFormat(format()) && !isSRGBFormat(outFormat) && !cpuSRGBConversion) {
		BEGIN_PROFILER_EVENT("G3D::Texture::toPixelTransferBuffer (slow path)");
		// Copy to non-srgb texture first, forcing OpenGL to perform the sRGB conversion in a pixel shader
		const shared_ptr<TextureDist>& temp = TextureDist::createEmpty("Temporary copy", m_width, m_height, outFormat, m_dimension, false, m_depth);
		Texture::copy(dynamic_pointer_cast<TextureDist>(const_cast<TextureDist*>(this)->shared_from_this()), temp);
		shared_ptr<GLPixelTransferBuffer> buffer = GLPixelTransferBuffer::create(m_width, m_height, outFormat);
		temp->toPixelTransferBuffer(buffer, outFormat, mipLevel, face);

		END_PROFILER_EVENT();
		return buffer;
	}

	BEGIN_PROFILER_EVENT("G3D::Texture::toPixelTransferBuffer");
	// OpenGL's sRGB readback is non-intuitive.  If we're reading from sRGB to sRGB, we actually read back using "RGB".
	if (outFormat == format()) {
		if (outFormat == ImageFormat::SRGB8()) {
			outFormat = ImageFormat::RGB8();
		}
		else if (outFormat == ImageFormat::SRGBA8()) {
			outFormat = ImageFormat::RGBA8();
		}
	}
	int mipDepth = 1;
	if (dimension() == DIM_3D) {
		mipDepth = depth() >> mipLevel;
	}
	else if (dimension() == DIM_2D_ARRAY) {
		mipDepth = depth();
	}

	BEGIN_PROFILER_EVENT("GLPixelTransferBuffer::create");
	shared_ptr<GLPixelTransferBuffer> buffer = GLPixelTransferBuffer::create(width() >> mipLevel, height() >> mipLevel, outFormat, nullptr, mipDepth, GL_STATIC_READ);
	END_PROFILER_EVENT();

	toPixelTransferBuffer(buffer, outFormat, mipLevel, face);
	END_PROFILER_EVENT();
	return buffer;
}

	shared_ptr<Image> toImage5(const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X) const {
		return Image::fromPixelTransferBuffer(Texture::toPixelTransferBuffer(outFormat, mipLevel, face));
	}
	
};