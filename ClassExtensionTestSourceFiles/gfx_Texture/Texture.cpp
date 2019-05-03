/**
  \file G3D-gfx.lib/source/Texture.cpp

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#include "G3D-base/Log.h"
#include "G3D-base/Any.h"
#include "G3D-base/Matrix3.h"
#include "G3D-base/Rect2D.h"
#include "G3D-base/fileutils.h"
#include "G3D-base/FileSystem.h"
#include "G3D-base/ImageFormat.h"
#include "G3D-base/CoordinateFrame.h"
#include "G3D-base/prompt.h"
#include "G3D-base/ImageConvert.h"
#include "G3D-base/CPUPixelTransferBuffer.h"
#include "G3D-base/format.h"
#include "G3D-base/CubeMap.h"
#include "G3D-gfx/glcalls.h"
#include "G3D-gfx/Texture.h"
#include "G3D-gfx/getOpenGLState.h"
#include "G3D-gfx/GLCaps.h"
#include "G3D-gfx/Framebuffer.h"
#include "G3D-gfx/RenderDevice.h"
#include "G3D-gfx/Shader.h"
#include "G3D-gfx/GLPixelTransferBuffer.h"
#include "G3D-app/BumpMap.h"
#include "G3D-app/GApp.h"
#include "G3D-app/VideoRecordDialog.h"

#ifdef verify
#undef verify
#endif

namespace G3D {

    
/**
 Legacy: sets the active texture to zero
 */
static void glStatePush() {
    glActiveTexture(GL_TEXTURE0);
}

/**
 Legacy: unneeded
 */
static void glStatePop() {
}

// From http://jcgt.org/published/0003/02/01/paper-lowres.pdf
// Returns +/-1
static Vector2 signNotZero(Vector2 v) {
    return Vector2((v.x >= 0.0f) ? +1.0f : -1.0f, (v.y >= 0.0f) ? +1.0f : -1.0f);
}

// Assume normalized input. Output is on [-1, 1] for each component.
static Vector2 float32x3_to_oct(Vector3 v) {
    // Project the sphere onto the octahedron, and then onto the xy plane
    const Vector2& p = v.xy() * (1.0f / (fabsf(v.x) + fabsf(v.y) + fabsf(v.z)));
    // Reflect the folds of the lower hemisphere over the diagonals
    return (v.z <= 0.0f) ? ((Vector2(1.0f, 1.0f) - abs(p.yx())) * signNotZero(p)) : p;
}

// From http://jcgt.org/published/0003/02/01/paper-lowres.pdf
static Vector3 oct_to_float32x3(Vector2 e) {
    Vector3 v(e.xy(), 1.0f - fabsf(e.x) - fabsf(e.y));

    if (v.z < 0) {
        v = Vector3((Vector2(1.0f, 1.0f) - abs(v.yx())) * signNotZero(v.xy()), v.z);
    }

    return v.direction();
}

// From http://jcgt.org/published/0003/02/01/paper-lowres.pdf
static Vector2 float32x3_to_octn_precise(Vector3 v, const int n) {
    Vector2 s = float32x3_to_oct(v); // Remap to the square

    // Each snorms max value interpreted as an integer,
    // e.g., 127.0 for snorm8
    const float M = float(1 << ((n / 2) - 1)) - 1.0f;

    // Remap components to snorm(n/2) precision...with floor instead
    // of round (see equation 1)
    s = (s.clamp(-1.0f, +1.0f) * M).floor() * (1.0f / M);
    Vector2 bestRepresentation = s;
    float highestCosine = dot(oct_to_float32x3(s), v);

    // Test all combinations of floor and ceil and keep the best.
    // Note that at +/- 1, this will exit the square... but that
    // will be a worse encoding and never win.
    for (int i = 0; i <= 1; ++i) {
        for (int j = 0; j <= 1; ++j) {
            // This branch will be evaluated at compile time
            if ((i != 0) || (j != 0)) {
                // Offset the bit pattern (which is stored in floating
                // point!) to effectively change the rounding mode
                // (when i or j is 0: floor, when it is one: ceiling)
                const Vector2 candidate = Vector2(float(i), float(j)) * (1.0f / M) + s;
                
                const float cosine = dot(oct_to_float32x3(candidate), v);
                if (cosine > highestCosine) {
                    bestRepresentation = candidate;
                    highestCosine = cosine;
                }
            }
        }
    }

    return bestRepresentation;
}

static GLenum dimensionToTarget(Texture::Dimension d, int numSamples = 1);

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
    const Texture::Encoding& encoding);

static void transform(const shared_ptr<Image>& image, const CubeMapConvention::CubeMapInfo::Face& info) {
    // Apply transformations
    if (info.flipX) {
        image->flipHorizontal();
    }
    if (info.flipY) {
        image->flipVertical();
    }
    if (info.rotations > 0) {
        image->rotateCW(toRadians(90.0 * info.rotations));
    }
}


static void computeStats
(const shared_ptr<PixelTransferBuffer>& ptb,
 Color4&      minval,
 Color4&      maxval,
 Color4&      meanval,
 AlphaFilter& alphaFilter,
 const Texture::Encoding& encoding);


void Texture::completeCPULoading() {
    debugAssert(notNull(m_loadingInfo));

    if (m_loadingInfo->nextStep == LoadingInfo::LOAD_FROM_DISK) {
        // Only the first MIP is used by the code path below
        m_loadingInfo->ptbArray.resize(1);
        Array<shared_ptr<PixelTransferBuffer>>& faceArray = m_loadingInfo->ptbArray[0];

        debugAssertM(m_loadingInfo->filename[0] != "<white>", "Pseudotextures should have been handled above");    
        if ((m_dimension == DIM_2D) || (m_dimension == DIM_3D)) {
            m_loadingInfo->ptbArray[0].resize(1);
            try {
                const shared_ptr<Image>& image = Image::fromBinaryInput(*m_loadingInfo->binaryInput);

                // Convert L8/R8 to RGB8 for OpenGL, unless bump map processing is going to happen and convert it anyway.
                if ((image->format() == ImageFormat::L8() || image->format() == ImageFormat::R8()) &&
                    (m_loadingInfo->preprocess.bumpMapPreprocess.mode == BumpMapPreprocess::Mode::NONE)) {
                    image->convertToRGB8();
                }
                faceArray[0] = image->toPixelTransferBuffer();
            } catch (const String& e) {
                throw Image::Error(e, m_loadingInfo->filename[0]);
            }
        } else {
            debugAssert(m_dimension == DIM_CUBE_MAP);
            faceArray.resize(6);
            // Load each cube face on a different thread
            runConcurrently(0, faceArray.size(), [&](int f) {
                // The first image was already loaded into memory
                // in compressed form in the binary input for metadata,
                // so reuse it here.
                const shared_ptr<Image>& image = (f == 0) ? Image::fromBinaryInput(*m_loadingInfo->binaryInput) : Image::fromFile(m_loadingInfo->filename[f]);
                if (image->format() == ImageFormat::L8() || image->format() == ImageFormat::R8()) {
                    image->convertToRGB8();
                }
                transform(image, m_loadingInfo->cubeMapInfo.face[f]);
                faceArray[f] = image->toPixelTransferBuffer();
            });
        }
        delete m_loadingInfo->binaryInput;
        m_loadingInfo->binaryInput = nullptr;
        
        if (m_encoding.format == ImageFormat::L8()) {
            // Don't let L8 textures slip by after loading
            m_encoding.format = m_loadingInfo->ptbArray[0][0]->format();
        }

        debugAssert(((m_loadingInfo->ptbArray[0][0]->format() != ImageFormat::L8()) && 
            (m_loadingInfo->ptbArray[0][0]->format() != ImageFormat::R8())) ||
            (m_loadingInfo->preprocess.bumpMapPreprocess.mode != BumpMapPreprocess::Mode::NONE));
        m_loadingInfo->nextStep = LoadingInfo::PREPROCESS;
    }

    // Note that nextStep will have advanced from above
    if (m_loadingInfo->nextStep == LoadingInfo::PREPROCESS) {
        debugAssertM(isNull(m_loadingInfo->binaryInput), "Input should have been deallocated by this point");
    
        debugAssert(m_width == m_loadingInfo->ptbArray[0][0]->width());
        debugAssert(m_height == m_loadingInfo->ptbArray[0][0]->height());    
    
        // Check for at least one miplevel on the incoming data
        const int numMipMaps = m_loadingInfo->ptbArray.length();
        debugAssert(numMipMaps > 0);
    
        m_detectedHint = AlphaFilter::DETECT;

        if (m_dimension == DIM_3D) {
            debugAssertM(numMipMaps == 1, "DIM_3D textures do not support mipmaps");
        } else if (m_dimension != DIM_3D && m_dimension != DIM_CUBE_MAP_ARRAY && m_dimension != DIM_2D_ARRAY) {
            debugAssertM(m_depth == 1, "Depth must be 1 for all textures that are not DIM_3D, DIM_CUBE_MAP_ARRAY, or DIM_2D_ARRAY");
        }

        if ((m_loadingInfo->preprocess.modulate != Color4::one()) || (m_loadingInfo->preprocess.offset != Color4::zero()) || (m_loadingInfo->preprocess.gammaAdjust != 1.0f) || m_loadingInfo->preprocess.convertToPremultipliedAlpha) {
            const ImageFormat* f = m_loadingInfo->ptbArray[0][0]->format();
            debugAssert((f->code == ImageFormat::CODE_RGB8) ||
                        (f->code == ImageFormat::CODE_RGBA8) ||
                        (f->code == ImageFormat::CODE_R8) ||
                        (f->code == ImageFormat::CODE_L8));

            // Allow brightening to fail silently in release mode
            if ((f->code == ImageFormat::CODE_R8) ||
                (f->code == ImageFormat::CODE_L8) ||
                (f->code == ImageFormat::CODE_R8) ||
                (f->code == ImageFormat::CODE_RGB8) ||
                (f->code == ImageFormat::CODE_RGBA8)) {

                // Copy the source array
                const int numBytes = iCeil(m_width * m_height * m_depth * f->cpuBitsPerPixel / 8.0f);
                for (int m = 0; m < numMipMaps; ++m) {
                    for (int f = 0; f < m_loadingInfo->ptbArray[m].size(); ++f) {
                        // No reference because we may assign to ptbArray below
                        const shared_ptr<PixelTransferBuffer> src = m_loadingInfo->ptbArray[m][f];
                        if (src->ownsMemory()) {
                            // Mutate in place
                            void* data = const_cast<void*>(src->mapReadWrite());
                            m_loadingInfo->preprocess.modulateOffsetAndGammaAdjustImage(m_loadingInfo->ptbArray[0][0]->format()->code, data, data, numBytes);
                        } else {
                            const shared_ptr<PixelTransferBuffer>& dst = CPUPixelTransferBuffer::create(m_width, m_height, src->format());
                            m_loadingInfo->preprocess.modulateOffsetAndGammaAdjustImage(m_loadingInfo->ptbArray[0][0]->format()->code, (void*)src->mapRead(), dst->mapWrite(), numBytes);
                            dst->unmap();
                            // Replace the source with the destination
                            m_loadingInfo->ptbArray[m][f] = dst;
                        }
                        src->unmap();
                    } // face
                } // mip
            }
        }

        debugAssertM(! ((m_loadingInfo->preprocess.bumpMapPreprocess.mode != BumpMapPreprocess::Mode::NONE) && m_loadingInfo->preprocess.convertToPremultipliedAlpha), "A texture should not be both a bump map and an alpha-masked value");

        if (m_loadingInfo->preprocess.bumpMapPreprocess.mode != BumpMapPreprocess::Mode::NONE) {
#ifdef G3D_DEBUG
            {
                const ImageFormat* f = m_loadingInfo->ptbArray[0][0]->format();
                debugAssertM(f->redBits == 8 || f->luminanceBits == 8, "To preprocess a texture with normal maps, 8-bit channels are required");
                debugAssertM(f->floatingPoint == false, "Cannot compute normal maps from floating point textures");
                debugAssertM(f->numComponents == 1 || f->numComponents == 3 || f->numComponents == 4, "1, 3, or 4 channels needed to compute normal maps");

                debugAssertM(f->compressed == false, "Cannot compute normal maps from compressed textures");
                debugAssertM(numMipMaps == 1, "Cannot specify mipmaps when computing normal maps automatically");
            }
            #endif

            bool computeNormal = false;
            bool computeBump   = false;

            bool hasNormal = false;
            bool hasBump   = false;
            bool bumpInRed = false;

            // Not a reference because we may mutate ptbArray below
            const shared_ptr<PixelTransferBuffer> src = m_loadingInfo->ptbArray[0][0];
            
            if ((m_loadingInfo->preprocess.bumpMapPreprocess.mode == BumpMapPreprocess::Mode::AUTODETECT_TO_NORMAL_AND_BUMP) ||
                (m_loadingInfo->preprocess.bumpMapPreprocess.mode == BumpMapPreprocess::Mode::AUTODETECT_TO_AUTODETECT)) {
                BumpMap::detectNormalBumpFormat(reinterpret_cast<const unorm8*>(src->mapRead()), src->format()->numComponents, m_width * m_height, hasBump, hasNormal, bumpInRed);
                src->unmap();
            }
        
            switch (m_loadingInfo->preprocess.bumpMapPreprocess.mode) {
            case BumpMapPreprocess::Mode::NONE:
                alwaysAssertM(false, "Should not reach this point");
                break;

            case BumpMapPreprocess::Mode::BUMP_TO_NORMAL_AND_BUMP:
                computeNormal = true;
                computeBump   = false;
                bumpInRed     = true;
                break;

            case BumpMapPreprocess::Mode::AUTODETECT_TO_NORMAL_AND_BUMP:
                if (hasBump && ! hasNormal) {
                    computeNormal = true;
                    computeBump = false;
                } else if (hasNormal && ! hasBump) {
                    // Compute the bump map (slow)
                    computeNormal = false;
                    computeBump = true;
                } else if (hasNormal && hasBump) {
                    computeNormal = false;
                    computeBump = false;
                } else {
                    debugAssertM(false, "AUTODETECT_TO_NORMAL_AND_BUMP texture has neither normal nor bump on input");
                }
                break;

            case BumpMapPreprocess::Mode::AUTODETECT_TO_AUTODETECT:
                 if (hasBump && ! hasNormal) {
                    computeNormal = true;
                    computeBump = false;
                } else if (hasNormal && ! hasBump) {
                    // Stick with the existing normal map
                    computeNormal = false;
                    computeBump = false;
                } else if (hasNormal && hasBump) {
                    // Nothing to do
                    computeNormal = false;
                    computeBump = false;
                } else {
                    // Nothing to do
                    computeNormal = false;
                    computeBump = false;
                }
                break;
            }

            if (computeNormal) {
                m_loadingInfo->ptbArray[0][0] = BumpMap::computeNormalMap(src->width(), src->height(), src->format()->numComponents, 
                                                           reinterpret_cast<const unorm8*>(src->mapRead()),
                                                           m_loadingInfo->preprocess.bumpMapPreprocess);
                src->unmap();
                m_encoding.format            = m_loadingInfo->ptbArray[0][0]->format();
                m_encoding.readMultiplyFirst = Color3::one() * 2.0f;
                m_encoding.readAddSecond     = -Color3::one();
            }

            if (computeBump) {
                debugAssertM(false, "Run-time bump map computation is not supported yet");
            }

            if (m_encoding.format == ImageFormat::AUTO()) {
                m_encoding.format = m_loadingInfo->ptbArray[0][0]->format();
            }
            
            debugAssertM((m_encoding.format->openGLBaseFormat == GL_LUMINANCE && computeNormal) ||
                (m_encoding.format->openGLBaseFormat == GL_RGBA) ||
                (m_encoding.format->openGLBaseFormat == GL_RGB),
                "Desired format must contain at least RGB channels for normal mapping");
        }

        if (m_encoding.format == ImageFormat::AUTO()) {
            if (m_loadingInfo->preferSRGBForAuto) {
                m_encoding.format = ImageFormat::getSRGBFormat(m_loadingInfo->ptbArray[0][0]->format());
            } else {
                m_encoding.format = m_loadingInfo->ptbArray[0][0]->format();
            }
        }

        if (m_loadingInfo->ptbArray[0][0]->format()->compressed) {
            m_encoding.format = m_loadingInfo->ptbArray[0][0]->format();
        }

        if (m_loadingInfo->preprocess.computeMinMaxMean && (m_loadingInfo->ptbArray[0].length() == 1)) {
            // Only do stat computation for single textures on MIP zero
            computeStats(m_loadingInfo->ptbArray[0][0], m_min, m_max, m_mean, m_detectedHint, m_encoding);
        }

        debugAssert(notNull(m_encoding.format));
    
        m_loadingInfo->nextStep = LoadingInfo::TRANSFER_TO_GPU;
    }
}


void Texture::completeGPULoading() {
    debugAssert(notNull(m_loadingInfo) && 
        (m_loadingInfo->nextStep >= LoadingInfo::TRANSFER_TO_GPU));

    debugAssertM(isNull(m_loadingInfo->binaryInput), "Input should have been deallocated by this point");

    if (m_loadingInfo->nextStep == LoadingInfo::TRANSFER_TO_GPU) {
        if (m_encoding.format == ImageFormat::AUTO()) {
            m_encoding.format = m_loadingInfo->ptbArray[0][0]->format();
        }

        debugAssert(notNull(m_encoding.format));
        if (! GLCaps::supportsTexture(m_encoding.format)) {
            if (m_encoding.format == ImageFormat::L8()) {
                m_encoding.format = ImageFormat::R8(); 
            } else {
                throw String("Unsupported texture format: ") + m_encoding.format->name();
            }
        }

        debugAssertM(GLCaps::supportsTexture(m_encoding.format), "Unsupported texture format: " + m_encoding.format->name());

        // Create the OpenGL texture
        m_textureID = newGLTextureID();

        debugAssertGLOk();
        const int numMipMaps = m_loadingInfo->ptbArray.size();
        glStatePush(); {
            int mipWidth   = m_width;
            int mipHeight  = m_height;
            int mipDepth   = m_depth;
 
            // Set unpacking alignment
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GLenum target = dimensionToTarget(m_dimension);
            glBindTexture(target, m_textureID);

            for (int mipLevel = 0; mipLevel < numMipMaps; ++mipLevel) {
                const int numFaces = m_loadingInfo->ptbArray[mipLevel].length();
            
                debugAssert(((m_dimension == DIM_CUBE_MAP) ? 6 : 1) == numFaces);
        
                for (int f = 0; f < numFaces; ++f) {
                    if (numFaces == 6) {
                        // Choose the appropriate face target
                        target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + f;
                    }

                    const shared_ptr<PixelTransferBuffer>& ptb = m_loadingInfo->ptbArray[mipLevel][f];
                    const uint8* bytesPtr = reinterpret_cast<const uint8*>(ptb->mapRead());
                    const ImageFormat* fmt = ptb->format();
                    if (fmt == ImageFormat::L8()) { fmt = ImageFormat::R8(); }

                    debugAssertGLOk();
                    createTexture
                        (target, 
                         bytesPtr, 
                         fmt->openGLFormat, 
                         fmt->openGLBaseFormat,
                         mipWidth, 
                         mipHeight, 
                         m_depth,
                         m_encoding.format->openGLFormat, 
                         fmt->cpuBitsPerPixel / 8, 
                         mipLevel, 
                         fmt->compressed, 
                         fmt->openGLDataFormat,
                         m_numSamples,
                         m_encoding);

                    ptb->unmap();
                    debugAssertGLOk();
                } // for each face

                mipWidth  = G3D::max(1, mipWidth / 2);
                mipHeight = G3D::max(1, mipHeight / 2);
                mipDepth  = G3D::max(1, mipDepth / 2);
            } // for each mip
   
        } glStatePop();

        debugAssertGLOk();

        if (isNaN(m_min.a)) {
            if (m_encoding.format->opaque) {
                m_min.a = m_max.a = m_mean.a = 1.0f;
            } else {
                m_min.a = 0.0f;
            }
        }
        m_opaque = (m_encoding.readMultiplyFirst.a * m_min.a) >= 1.0f;
        m_hasMipMaps = false;
        m_appearsInTextureBrowserWindow = true;
        m_destroyGLTextureInDestructor = true;

        debugAssertGLOk();
        if (m_loadingInfo->generateMipMaps && (numMipMaps == 1)) {
            // Generate mipmaps for textures requiring them
            glBindTexture(openGLTextureTarget(), m_textureID);
            glGenerateMipmap(openGLTextureTarget());
            m_hasMipMaps = true;
        } else if (numMipMaps > 1) {
            m_hasMipMaps = true;
        }

        debugAssertGLOk();

        m_destroyGLTextureInDestructor = true;
        m_appearsInTextureBrowserWindow = true;

        m_loadingInfo->nextStep = LoadingInfo::SET_SAMPLER_PARAMETERS;
    }
    

    if (m_loadingInfo->nextStep == LoadingInfo::SET_SAMPLER_PARAMETERS) {
        m_cachedSamplerSettings = Sampler(WrapMode::TILE, InterpolateMode::NEAREST_NO_MIPMAP);

        debugAssert(m_encoding.format);
        debugAssertGLOk();

        GLenum target = dimensionToTarget(m_dimension, m_numSamples);
        glBindTexture(target, m_textureID);
        setAllSamplerParameters(target, m_cachedSamplerSettings);        
        glBindTexture(target, GL_ZERO);
        debugAssertGLOk();

        m_sizeOfAllTexturesInMemory += sizeInMemory();
        m_loadingInfo->nextStep = LoadingInfo::DONE;
        m_needsForce = false;

        delete m_loadingInfo;
        m_loadingInfo = nullptr;
    }
}


void Texture::force() const {
    // Quick, mutex-less conservative out for the common run-time case
    if (! m_needsForce) { return; }

    m_loadingMutex.lock(); {
        // Check for race condition
        if (! m_needsForce) { m_loadingMutex.unlock(); return; }

        debugAssert(notNull(m_loadingInfo));
        debugAssert(notNull(m_loadingThread));

        // Block on the actual loading operation
        m_loadingThread->join();
        // Free the callback code
        delete m_loadingThread;
        m_loadingThread = nullptr;       
        // Upload to GL
        const_cast<Texture*>(this)->completeGPULoading();

        debugAssert(isNull(m_loadingInfo));
        m_needsForce = false;
    } m_loadingMutex.unlock();
}


shared_ptr<Texture> Texture::cosHemiRandom() {
    static shared_ptr<Texture> t;

    if (isNull(t)) {
        Random rnd;
        const shared_ptr<GLPixelTransferBuffer>& ptb = GLPixelTransferBuffer::create(1024, 1024, ImageFormat::RG32F());
        {
            Vector2* ptr = (Vector2*)ptb->mapWrite();
            for (int i = 0; i < ptb->width() * ptb->height(); ++i) {
                Vector3 v;
                rnd.cosHemi(v.x, v.y, v.z);

                ptr[i] = float32x3_to_octn_precise(v, 32);
            }
            ptr = nullptr;
            ptb->unmap();
        }
        t = Texture::fromPixelTransferBuffer("G3D::Texture::cosHemiRandom", ptb, ImageFormat::RG16_SNORM(), Texture::DIM_2D, false);
        t->visualization.max =  1.0f;
        t->visualization.min = -1.0f;
        t->visualization.documentGamma = 2.1f;
    }

    return t;
}


shared_ptr<Texture> Texture::sphereRandom() {
    static shared_ptr<Texture> t;

    if (isNull(t)) {
        Random rnd;
        const shared_ptr<GLPixelTransferBuffer>& ptb = GLPixelTransferBuffer::create(1024, 1024, ImageFormat::RG32F());
        {
            Vector2* ptr = (Vector2*)ptb->mapWrite();
            for (int i = 0; i < ptb->width() * ptb->height(); ++i) {
                Vector3 v;
                rnd.sphere(v.x, v.y, v.z);

                ptr[i] = float32x3_to_octn_precise(v, 32);
            }
            ptr = nullptr;
            ptb->unmap();
        }
        t = Texture::fromPixelTransferBuffer("G3D::Texture::sphereRandom", ptb, ImageFormat::RG16_SNORM(), Texture::DIM_2D, false);
        t->visualization.max =  1.0f;
        t->visualization.min = -1.0f;
        t->visualization.documentGamma = 2.1f;
    }

    return t;
}


shared_ptr<Texture> Texture::uniformRandom() {
    static shared_ptr<Texture> t;

    if (isNull(t)) {
        Random rnd;
        const shared_ptr<GLPixelTransferBuffer>& ptb = GLPixelTransferBuffer::create(1024, 1024, ImageFormat::RG16());
        {
            snorm16* ptr = (snorm16*)ptb->mapWrite();
            for (int i = 0; i < ptb->width() * ptb->height() * 2; ++i) {
                ptr[i] = snorm16(rnd.uniform()); ++i;
                ptr[i] = snorm16(rnd.uniform());
            }
            ptr = nullptr;
            ptb->unmap();
        }
        t = Texture::fromPixelTransferBuffer("G3D::Texture::uniformRandom", ptb, ImageFormat::RG16(), Texture::DIM_2D, false);
        t->visualization.max = 1.0f;
        t->visualization.min = 0.0f;
        t->visualization.documentGamma = 2.1f;
    }

    return t;
}

//////////////////////////////////////////////////////////

void Texture::clearCache() {
    s_cache.clear();
}

WeakCache<uintptr_t, shared_ptr<Texture> > Texture::s_allTextures;

WeakCache<Texture::Specification, shared_ptr<Texture> > Texture::s_cache;

shared_ptr<Texture> Texture::getTextureByName(const String& name) {
    Array<shared_ptr<Texture> > allTextures;
    getAllTextures(allTextures);
    for (int i = 0; i < allTextures.size(); ++i) {
        const shared_ptr<Texture>& t = allTextures[i];
        if (t->name() == name) {
            return t;
        }
    }
    return nullptr;
}


size_t Texture::Specification::hashCode() const {
    return HashTrait<String>::hashCode(filename) ^ HashTrait<String>::hashCode(alphaFilename); 
}

/** Used by various Texture methods when a framebuffer is needed */
static const shared_ptr<Framebuffer>& workingFramebuffer() {
    static shared_ptr<Framebuffer> fbo = Framebuffer::create("Texture workingFramebuffer");
    return fbo;
}

   
void Texture::getAllTextures(Array<shared_ptr<Texture> >& textures) {
    s_allTextures.getValues(textures);
}

    
void Texture::getAllTextures(Array<weak_ptr<Texture> >& textures) {
    Array<shared_ptr<Texture> > sharedTexturePointers;
    getAllTextures(sharedTexturePointers);
    for (int i = 0; i < sharedTexturePointers.size(); ++i) {
        textures.append(sharedTexturePointers[i]);
    }
    
}


Color4 Texture::readTexel(int x, int y, RenderDevice* rd, int mipLevel, int z, CubeFace face) const {
    force();
    debugAssertGLOk();
    const shared_ptr<Framebuffer>& fbo = workingFramebuffer();

    if (isNull(rd)) {
        rd = RenderDevice::current;
    }

    Color4 c;

    // Read back 1 pixel
    const shared_ptr<Texture>& me = dynamic_pointer_cast<Texture>(const_cast<Texture*>(this)->shared_from_this());
    bool is3D = dimension() == DIM_2D_ARRAY || dimension() == DIM_3D || dimension() == DIM_CUBE_MAP_ARRAY;
    int layer = is3D ? z : -1;
    if (format()->isIntegerFormat()) {
        int ints[4];
        fbo->set(Framebuffer::COLOR0, me, face, mipLevel, layer);
        rd->pushState(fbo);
        glReadPixels(x, y, 1, 1, GL_RGBA_INTEGER, GL_INT, ints);
        c = Color4(float(ints[0]), float(ints[1]), float(ints[2]), float(ints[3]));
        rd->popState();
    } else if (format()->depthBits == 0) {
        fbo->set(Framebuffer::COLOR0, me, face, mipLevel, layer);
        rd->pushState(fbo);
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, &c);
        rd->popState();
    } else {
        // This is a depth texture
        fbo->set(Framebuffer::DEPTH, me, face, mipLevel, layer);
        rd->pushState(fbo);
        glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &c.r);
        rd->popState();
        c.g = c.b = c.a = c.r;
    }
    fbo->clear();
    return c;
}


const CubeMapConvention::CubeMapInfo& Texture::cubeMapInfo(CubeMapConvention convention) {
    static CubeMapConvention::CubeMapInfo cubeMapInfo[CubeMapConvention::COUNT];
    static bool initialized = false;
    if (! initialized) {
        initialized = true;
        cubeMapInfo[CubeMapConvention::QUAKE].name = "Quake";
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_X].suffix = "bk";

        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_X].suffix = "ft";

        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_Y].suffix = "up";

        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_Y].suffix = "dn";

        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::POS_Z].suffix = "rt";

        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::QUAKE].face[CubeFace::NEG_Z].suffix = "lf";


        cubeMapInfo[CubeMapConvention::UNREAL].name = "Unreal";
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_X].suffix = "east";

        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_X].suffix = "west";

        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_Y].suffix = "up";

        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_Y].suffix = "down";

        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::POS_Z].suffix = "south";

        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::UNREAL].face[CubeFace::NEG_Z].suffix = "north";


        cubeMapInfo[CubeMapConvention::G3D].name = "G3D";
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_X].suffix = "+x";

        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_X].suffix = "-x";

        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_Y].suffix = "+y";

        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_Y].suffix = "-y";

        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::POS_Z].suffix = "+z";

        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::G3D].face[CubeFace::NEG_Z].suffix = "-z";


        cubeMapInfo[CubeMapConvention::DIRECTX].name = "DirectX";
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_X].suffix = "PX";

        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_X].flipX  = true;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_X].flipY  = false;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_X].suffix = "NX";

        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_Y].suffix = "PY";

        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_Y].flipX  = true;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_Y].flipY  = false;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_Y].suffix = "NY";

        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::POS_Z].suffix = "PZ";

        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_Z].flipX  = true;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_Z].flipY  = false;
        cubeMapInfo[CubeMapConvention::DIRECTX].face[CubeFace::NEG_Z].suffix = "NZ";
    }

    return cubeMapInfo[convention];
}


CubeMapConvention Texture::determineCubeConvention(const String& filename) {
    String filenameBase, filenameExt;
    Texture::splitFilenameAtWildCard(filename, filenameBase, filenameExt);
    if (FileSystem::exists(filenameBase + "east" + filenameExt)) {
        return CubeMapConvention::UNREAL;
    } else if (FileSystem::exists(filenameBase + "lf" + filenameExt)) {
        return CubeMapConvention::QUAKE;
    } else if (FileSystem::exists(filenameBase + "+x" + filenameExt)) {
        return CubeMapConvention::G3D;
    } else if (FileSystem::exists(filenameBase + "PX" + filenameExt) || FileSystem::exists(filenameBase + "px" + filenameExt)) {
        return CubeMapConvention::DIRECTX;
    }
    throw String("File not found");
    return CubeMapConvention::G3D;
}


static void generateCubeMapFilenames(const String& src, String realFilename[6], CubeMapConvention::CubeMapInfo& info) {
    String filenameBase, filenameExt;
    Texture::splitFilenameAtWildCard(src, filenameBase, filenameExt);

    CubeMapConvention convention = Texture::determineCubeConvention(src);

    info = Texture::cubeMapInfo(convention);
    for (int f = 0; f < 6; ++f) {
        realFilename[f] = filenameBase + info.face[f].suffix + filenameExt;
    }
}


int64 Texture::m_sizeOfAllTexturesInMemory = 0;


shared_ptr<Texture> Texture::singleChannelDifference
   (RenderDevice* rd,
    const shared_ptr<Texture>& t0,
    const shared_ptr<Texture>& t1,
    int channel) {

    debugAssertM(t0->width() == t1->width() && t0->height() == t1->height(), "singleChannelDifference requires the input textures to be of the same size");
    debugAssertM(channel >= 0 && channel < 4, "singleChannelDifference requires the input textures to be of the same size");
    shared_ptr<Framebuffer> fb = Framebuffer::create(Texture::createEmpty(t0->name() + "-" + t1->name(), t0->width(), t0->height(), ImageFormat::RG32F()));
    rd->push2D(fb); {
        Args args;
        args.setUniform("input0_buffer", t0, Sampler::buffer());
        args.setUniform("input1_buffer", t1, Sampler::buffer());
        args.setMacro("CHANNEL", channel);
        args.setRect(rd->viewport());
        LAUNCH_SHADER_WITH_HINT("Texture_singleChannelDiff.*", args, t0->name() + "->" + t1->name());
    } rd->pop2D();
    return fb->texture(0);
}


/** Creates a 4x4 PTB */
static shared_ptr<CPUPixelTransferBuffer> solidColorPTB(const Color4unorm8 c, const ImageFormat* fmt = ImageFormat::RGBA8()) {
    const shared_ptr<CPUPixelTransferBuffer>& imageBuffer = CPUPixelTransferBuffer::create(4, 4, fmt);
    const int n = imageBuffer->width() * imageBuffer->height();
    Color4unorm8* p = (Color4unorm8*)imageBuffer->buffer();
    for (int i = 0; i < n; ++i) {
        p[i] = c;
    }
    return imageBuffer;
}


const shared_ptr<Texture>& Texture::white() {
    static shared_ptr<Texture> t;
    if (isNull(t)) {
        // Cache is empty
        const shared_ptr<CPUPixelTransferBuffer>& imageBuffer = solidColorPTB(Color4unorm8(Color3::white()));
        t = Texture::fromPixelTransferBuffer("G3D::Texture::white", imageBuffer, imageBuffer->format(), Texture::DIM_2D);
        debugAssert(t->opaque());
        debugAssert(t->min() == Color4::one());
        debugAssert(t->max() == Color4::one());
    }

    return t;
}


const shared_ptr<Texture>& Texture::opaqueBlackCube() {
    static shared_ptr<Texture> t;
    if (isNull(t)) {
        // Cache is empty
        const shared_ptr<CPUPixelTransferBuffer>& imageBuffer = solidColorPTB(Color4unorm8(Color3::black()));
        Array< Array<const void*> > bytes;
        Array<const void*>& cubeFace = bytes.next();
        for (int i = 0; i < 6; ++i)  {
            cubeFace.append(imageBuffer->buffer());
        }
        t = Texture::fromMemory("G3D::Texture::opaqueBlackCube", bytes, imageBuffer->format(), imageBuffer->width(), imageBuffer->height(), 1, 1, ImageFormat::RGB8(), Texture::DIM_CUBE_MAP);
        debugAssert(t->opaque());
    }

    return t;
}


const shared_ptr<Texture>& Texture::whiteCube() {
    static shared_ptr<Texture> t;
    if (isNull(t)) {
        // Cache is empty
        const shared_ptr<CPUPixelTransferBuffer>& imageBuffer = solidColorPTB(Color4unorm8(Color3::white()));
        Array< Array<const void*> > bytes;
        Array<const void*>& cubeFace = bytes.next();
        for (int i = 0; i < 6; ++i)  {
            cubeFace.append(imageBuffer->buffer());
        }
        t = Texture::fromMemory("G3D::Texture::whiteCube", bytes, imageBuffer->format(), imageBuffer->width(), imageBuffer->height(), 1, 1, ImageFormat::RGB8(), Texture::DIM_CUBE_MAP);
        debugAssert(t->opaque());
    }

    return t;
}


shared_ptr<Texture> Texture::createColorCube(const Color4& color) {
    // Get the white cube and then make another texture using the same handle 
    // and a different encoding.
    const shared_ptr<Texture>& w = whiteCube();

    Encoding e;
    e.format = w->encoding().format;
    e.readMultiplyFirst = color;
    return fromGLTexture(color.toString(), w->openGLID(), e, AlphaFilter::ONE, DIM_CUBE_MAP);
}


const shared_ptr<Texture>& Texture::zero(Dimension d) {
    alwaysAssertM(d == DIM_2D || d == DIM_3D || d == DIM_2D_ARRAY, "Dimension must be 2D, 3D, or 2D Array");
    static Table<int, shared_ptr<Texture> > textures;
    if (!textures.containsKey(d)) {
        // Cache is empty                                                                                      
        const shared_ptr<CPUPixelTransferBuffer>& imageBuffer = solidColorPTB(Color4unorm8::zero());            
        textures.set(d, Texture::fromPixelTransferBuffer("G3D::Texture::zero", imageBuffer, imageBuffer->format(), d));
    }   

    return textures[d];
}


const shared_ptr<Texture>& Texture::opaqueBlack(Dimension d) {
    alwaysAssertM(d == DIM_2D || d == DIM_3D || d == DIM_2D_ARRAY, "Dimension must be 2D, 3D, or 2D Array");
    static Table<int, shared_ptr<Texture> > textures;

    bool b = false;
    shared_ptr<Texture>& t = textures.getCreate(d, b);
    if (b) {
        // Cache is empty                                                                                      
        const shared_ptr<CPUPixelTransferBuffer>& imageBuffer = solidColorPTB(Color4unorm8(Color3::black()));
        t = Texture::fromPixelTransferBuffer("G3D::Texture::opaqueBlack", imageBuffer, imageBuffer->format(), d);

        // Make obvious to UniversalBSDF that this is entirely black
        t->m_encoding.readMultiplyFirst = Color4(Color3::zero(), 1.0f);

        debugAssert(t->opaque());
    }
    
    return t;
}


const shared_ptr<Texture>& Texture::opaqueGray() {
    static shared_ptr<Texture> t;
    if (isNull(t)) {
        // Cache is empty                                                                                      
        const shared_ptr<CPUPixelTransferBuffer>& imageBuffer = solidColorPTB(Color4unorm8(Color4(0.5f, 0.5f, 0.5f, 1.0f)), ImageFormat::RGBA8());
        t = Texture::fromPixelTransferBuffer("Gray", imageBuffer);
        debugAssert(t->opaque());
    }
    
    return t;
}


void Texture::generateMipMaps() {
    force();
    glBindTexture(openGLTextureTarget(), openGLID());
    glGenerateMipmap(openGLTextureTarget());
    m_hasMipMaps = true;
}


#if 0
void Texture::uploadImages(const MIPCubeFacePTBArray& mipsPerCubeFace) {
    glBindTexture(openGLTextureTarget(), m_textureID);
    debugAssertGLOk();

    for (int mipIndex = 0; mipIndex < mipsPerCubeFace[0].length(); ++mipIndex) {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        shared_ptr<PixelTransferBuffer>   buffer   = mipsPerCubeFace[0][mipIndex];
        shared_ptr<GLPixelTransferBuffer> glBuffer = dynamic_pointer_cast<GLPixelTransferBuffer>(buffer);

        if (notNull(glBuffer)) {
            // Direct GPU->GPU transfer
            glBuffer->bindRead();
            glTexImage2D(openGLTextureTarget(), mipIndex, format()->openGLFormat, m_width, m_height, 0, mipsPerCubeFace[0][mipIndex]->format()->openGLBaseFormat, mipsPerCubeFace[0][mipIndex]->format()->openGLDataFormat, 0);
            glBuffer->unbindRead();
        } else {
            // Any->GPU transfer
            const void* ptr = buffer->mapRead();
            glTexImage2D(openGLTextureTarget(), mipIndex, format()->openGLFormat, m_width, m_height, 0, mipsPerCubeFace[0][mipIndex]->format()->openGLBaseFormat, mipsPerCubeFace[0][mipIndex]->format()->openGLDataFormat, ptr);
            mipsPerCubeFace[0][mipIndex]->unmap();
        }

        debugAssertGLOk();
    }
}
#endif


shared_ptr<Texture> Texture::fromMemory
   (const String&                   name,
    const void*                     bytes,
    const class ImageFormat*        bytesFormat,
    int                             width,
    int                             height,
    int                             depth,
    int                             numSamples,
    Encoding                        desiredEncoding,
    Dimension                       dimension,
    bool                            generateMipMaps,
    const Preprocess&               preprocess,
    bool                            preferSRGBForAuto) {

    const shared_ptr<Texture>& t = createShared<Texture>(name, width, height, depth, dimension, desiredEncoding, numSamples, false);  
    s_allTextures.set((uintptr_t)t.get(), t);

    t->m_conservativelyHasUnitAlpha = (bytesFormat->alphaBits == 0) &&
        (desiredEncoding.readMultiplyFirst.a + desiredEncoding.readAddSecond.a >= 1.0f);
    t->m_conservativelyHasNonUnitAlpha = (bytesFormat->alphaBits > 0) ||
        (desiredEncoding.readMultiplyFirst.a + desiredEncoding.readAddSecond.a < 1.0f);

    if (t->m_conservativelyHasUnitAlpha) {
        t->m_opaque = true;
    } else if (t->m_conservativelyHasNonUnitAlpha) {
        t->m_opaque = false;
    }

    debugAssert(!(t->m_conservativelyHasUnitAlpha && t->m_conservativelyHasNonUnitAlpha));
    // Convert to PixelTransferBuffers using the same memory

    t->m_loadingInfo = new LoadingInfo(LoadingInfo::PREPROCESS);

    LoadingInfo& info = *t->m_loadingInfo;
    info.ptbArray.resize(1);
    info.ptbArray[0].append(CPUPixelTransferBuffer::fromData(width, height, bytesFormat, (void*)bytes, depth));

    info.desiredEncoding   = desiredEncoding;
    // Because the data are shared, we cannot lazy load
    info.lazyLoadable      = false;
    info.generateMipMaps   = generateMipMaps;
    info.preferSRGBForAuto = preferSRGBForAuto;

    t->completeCPULoading();
    t->completeGPULoading();

    return t;
}


shared_ptr<Texture> Texture::fromGLTexture
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


shared_ptr<Texture> Texture::loadTextureFromSpec(const Texture::Specification& s) {
    shared_ptr<Texture> t;    

    if (s.alphaFilename.empty()) {
        t = Texture::fromFile(s.filename, s.encoding, s.dimension, s.generateMipMaps, s.preprocess, s.assumeSRGBSpaceForAuto);
    } else {
        t = Texture::fromTwoFiles(s.filename, s.alphaFilename, s.encoding, s.dimension, s.generateMipMaps, s.preprocess, s.assumeSRGBSpaceForAuto, false);
    }

    if ((s.filename == "<white>" || s.filename.empty()) && (! s.encoding.readMultiplyFirst.isOne() || ! s.encoding.readAddSecond.isZero())) {
        t->m_name = String("Color4") + (s.encoding.readMultiplyFirst + s.encoding.readAddSecond).toString();
        t->m_appearsInTextureBrowserWindow = false;
    }

    if (! s.name.empty()) {
        t->m_name = s.name;
    }

    return t;
}


Texture::TexelType Texture::texelType() const {
    const ImageFormat* f = format();
    if (f->numberFormat == ImageFormat::INTEGER_FORMAT) {
        if (f->openGLDataFormat == GL_UNSIGNED_BYTE ||
            f->openGLDataFormat == GL_UNSIGNED_SHORT ||
            f->openGLDataFormat == GL_UNSIGNED_INT) {
            return TexelType::UNSIGNED_INTEGER;
        } else {
            return TexelType::INTEGER;
        }
    }
    return TexelType::FLOAT;
}

    
shared_ptr<Texture> Texture::create(const Specification& s) {
    if (s.cachable) {
        if ((s.filename == "<white>" || s.filename.empty()) && s.alphaFilename.empty() && (s.dimension == DIM_2D) && s.encoding.readMultiplyFirst.isOne() && s.encoding.readAddSecond.isZero()) {
            // Make a single white texture when the other properties don't matter
            return Texture::white();
        } else if (((s.filename == "<white>") || s.filename.empty()) &&
                   s.alphaFilename.empty() &&
                   (s.dimension == DIM_2D) &&
                   (s.encoding.readMultiplyFirst.rgb() == Color3::zero()) &&
                   ((s.encoding.format == ImageFormat::AUTO()) || 
                    (s.encoding.format->alphaBits == 0)) &&
                   (s.encoding.readMultiplyFirst.a == 1.0f) &&
                    s.encoding.readAddSecond.isZero()) {
            // Make a single opaque black texture when the other properties don't matter
            return Texture::opaqueBlack();
        } else {
            shared_ptr<Texture> cachedValue = s_cache[s];
            if (isNull(cachedValue)) {
                cachedValue = loadTextureFromSpec(s);
                s_cache.set(s, cachedValue);
            }
            return cachedValue;
        }
    } else {
        return loadTextureFromSpec(s);
    }
}


Texture::Texture
    (const String&                  name,
     int                            width,
     int                            height,
     int                            depth,
     Dimension                      dimension,
     const Encoding&                encoding,
     int                            numSamples,
     bool                           needsForce) :
        m_name(name),
        m_dimension(dimension),
        m_encoding(encoding),
        m_width(width),
        m_height(height),
        m_depth(depth),
        m_min(Color4::nan()),
        m_max(Color4::nan()),
        m_mean(Color4::nan()),
        m_numSamples(numSamples),
        m_needsForce(needsForce) {
}


shared_ptr<Texture> Texture::fromFile
   (const String&                   filenameSpec,
    Encoding                        desiredEncoding,
    Dimension                       dimension,
    bool                            generateMipMaps,
    const Preprocess&               preprocess,
    bool                            preferSRGBSpaceForAuto) {
    
    if (endsWith(toLower(filenameSpec), ".exr") && (desiredEncoding.format == ImageFormat::AUTO())) {
        desiredEncoding.format = ImageFormat::RGBA32F();
    }
    
    if (dimension == DIM_2D_ARRAY) {
        // Handle the uncommon 2D array case separately from the
        // optimized path.
        Array<String> files;
        FileSystem::getFiles(filenameSpec, files, true);
        files.sort();

        Array<shared_ptr<Image>> images;
        images.resize(files.length());
        runConcurrently(0, images.size(), [&](int i) {
            images[i] = Image::fromFile(files[i]);
        });

        return Texture::fromPixelTransferBuffer(String("file: ") + FilePath::base(filenameSpec), Image::arrayToPixelTransferBuffer(images), desiredEncoding.format, dimension);
    }


    LoadingInfo* loadingInfo = new LoadingInfo(LoadingInfo::UNINITIALIZED);
    const int numFaces = (dimension == DIM_CUBE_MAP) ? 6 : 1;
    loadingInfo->filename[0] = filenameSpec;

    // Ensure that this is not "<white>" before splitting names
    if ((numFaces == 6) && ! beginsWith(filenameSpec, "<") && ! filenameSpec.empty()) {
        // Parse the filename into a base name and extension
        generateCubeMapFilenames(filenameSpec, loadingInfo->filename, loadingInfo->cubeMapInfo);
    }
        
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Handle pseudo-texture <white> cases, which do not touch disk
    if ((toLower(loadingInfo->filename[0]) == "<whitecube>") || (toLower(loadingInfo->filename[0]) == "<white>") || loadingInfo->filename[0].empty()) {
        debugAssertM(preprocess.modulate == Color4::one() && preprocess.offset == Color4::zero(), "Cannot preprocess when loading the <white> texture");
        const shared_ptr<PixelTransferBuffer>& first = solidColorPTB(Color4unorm8(Color3::white()));
        const shared_ptr<Texture>& instance =
            createShared<Texture>(String("file: ") + FilePath::base(filenameSpec), first->width(), first->height(), first->depth(), 
                dimension, desiredEncoding, 1, false);
        instance->m_conservativelyHasNonUnitAlpha = (desiredEncoding.readMultiplyFirst.a + desiredEncoding.readAddSecond.a < 1.0f);
        instance->m_conservativelyHasUnitAlpha = (desiredEncoding.readMultiplyFirst.a + desiredEncoding.readAddSecond.a >= 1.0f);
        instance->m_loadingInfo = loadingInfo;
        instance->m_loadingInfo->nextStep = LoadingInfo::TRANSFER_TO_GPU;
        
        loadingInfo->ptbArray.resize(1);
        loadingInfo->ptbArray[0].append(first);
        loadingInfo->generateMipMaps = generateMipMaps;
        loadingInfo->preprocess = preprocess;
        loadingInfo->preprocess.computeMinMaxMean = true;

        if (dimension == DIM_CUBE_MAP) {
            for (int f = 1; f < 6; ++f) {
                loadingInfo->ptbArray[0].append(solidColorPTB(Color4unorm8(Color3::white())));
            }
        }

        // Launch loader
        instance->completeCPULoading();
        instance->completeGPULoading();
        instance->m_min = instance->m_mean = instance->m_max = Color4::one();

        if (instance->m_encoding.readMultiplyFirst.a + instance->m_encoding.readAddSecond.a >= 1.0f) {
            instance->m_opaque = true;
            instance->m_detectedHint = AlphaFilter::ONE;
        }

        return instance;
    } // if white


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Lazy-loading case
    debugAssertM(loadingInfo->filename[0] != "<white>", "Pseudotextures should have been handled above");    

    loadingInfo->nextStep = LoadingInfo::LOAD_FROM_DISK;
    // Pull the dimensions from the metadata
    loadingInfo->binaryInput = new BinaryInput(loadingInfo->filename[0], G3D::G3D_LITTLE_ENDIAN);
    loadingInfo->lazyLoadable = true;

    int width, height, depth = 1;
    const ImageFormat* format = nullptr;
    const bool success = Image::metaDataFromBinaryInput(*loadingInfo->binaryInput, width, height, format);
    if (! success) {
        delete loadingInfo;
        throw Image::Error("Could not process image file format", loadingInfo->filename[0]);
    }

    if (desiredEncoding.format == ImageFormat::AUTO()) {
        desiredEncoding.format = preferSRGBSpaceForAuto ? ImageFormat::getSRGBFormat(format) : format;
    }

    // Allocate instance now and then push all other work else to another thread
    const shared_ptr<Texture>& instance = createShared<Texture>(String("file: ") + FilePath::base(filenameSpec), width, height, depth, dimension, desiredEncoding, 1, true);
    instance->m_loadingInfo = loadingInfo;
    loadingInfo->preprocess = preprocess;
    loadingInfo->preferSRGBForAuto = preferSRGBSpaceForAuto;
    loadingInfo->generateMipMaps = generateMipMaps;
    instance->m_conservativelyHasNonUnitAlpha = (desiredEncoding.format->alphaBits > 0) ||
        (desiredEncoding.readMultiplyFirst.a + desiredEncoding.readAddSecond.a < 1.0f);
    instance->m_conservativelyHasUnitAlpha = (desiredEncoding.format->alphaBits == 0) &&
        (desiredEncoding.readMultiplyFirst.a + desiredEncoding.readAddSecond.a >= 1.0f);
    s_allTextures.set((uintptr_t)instance.get(), instance);

    debugAssert(instance->m_needsForce && instance->m_loadingInfo->lazyLoadable);

    // For debugging Texture loading itself without threads
    static const bool debugForceEagerLoad = false;

    if (debugForceEagerLoad) {
        instance->completeCPULoading();
        instance->completeGPULoading();
    } else {
        instance->m_loadingThread = new std::thread([instance]() { instance->completeCPULoading(); });
#       if defined(G3D_WINDOWS) && 0
            String sname = String("Loading ") + instance->m_name;
            std::wstring wname(sname.begin(), sname.end());
            SetThreadDescription(instance->m_loadingThread->native_handle(), wname.c_str());
#       endif
    }

    return instance;
}


String Texture::caption() const {
    if (m_caption.empty()) {
        if (beginsWith(m_name, "file: ")) {
            return trimWhitespace(m_name.substr(5));
        } else {
            return m_name;
        }
    } else {
        return m_caption;
    }
}


shared_ptr<Texture> Texture::fromTwoFiles
   (const String&           filename,
    const String&           alphaFilename,
    Encoding                desiredEncoding,
    Dimension               dimension,
    bool                    generateMipMaps,
    const Preprocess&       preprocess,
    const bool              preferSRGBSpaceForAuto,
    const bool              useAlpha) {

    // The six cube map faces, or the one texture and 5 dummys.
    MIPCubeFacePointerArray mip;
    mip.resize(1);
    Array<const void*>& array = mip[0];

    const int numFaces = (dimension == DIM_CUBE_MAP) ? 6 : 1;

    array.resize(numFaces);
    for (int i = 0; i < numFaces; ++i) {
        array[i] = nullptr;
    }

    // Parse the filename into a base name and extension
    String filenameArray[6];
    String alphaFilenameArray[6];
    filenameArray[0] = filename;
    alphaFilenameArray[0] = alphaFilename;

    // Test for DIM_CUBE_MAP
    CubeMapConvention::CubeMapInfo info, alphaInfo;
    if (numFaces == 6) {
        generateCubeMapFilenames(filename, filenameArray, info);
        generateCubeMapFilenames(alphaFilename, alphaFilenameArray, alphaInfo);
    }
    
    shared_ptr<Image> color[6];
    shared_ptr<Image> alpha[6];
    shared_ptr<PixelTransferBuffer> buffers[6];
    shared_ptr<Texture> t;

    try {
        for (int f = 0; f < numFaces; ++f) {
            // Compose the two images to a single RGBA
            alpha[f] = Image::fromFile(alphaFilenameArray[f]);
            if ( !((toLower(filenameArray[f]) == "<white>") || filenameArray[f].empty() )) {
                color[f] = Image::fromFile(filenameArray[f]);
            } 

            const shared_ptr<CPUPixelTransferBuffer>& b = CPUPixelTransferBuffer::create(alpha[f]->width(), alpha[f]->height(), ImageFormat::RGBA8());
            uint8* newMap = reinterpret_cast<uint8*>(b->mapWrite());

            if (notNull(color[f])) {
                if (numFaces > 1) {
                    transform(color[f], info.face[f]);
                    transform(alpha[f], alphaInfo.face[f]);
                }
                const shared_ptr<PixelTransferBuffer>& cbuf = color[f]->toPixelTransferBuffer();
                const uint8* colorMap                   = reinterpret_cast<const uint8*>(cbuf->mapRead());
                const shared_ptr<PixelTransferBuffer>& abuf = alpha[f]->toPixelTransferBuffer();
                const uint8* alphaMap                   = reinterpret_cast<const uint8*>(abuf->mapRead());
            

                alwaysAssertM((color[f]->width()  == alpha[f]->width()) && 
                              (color[f]->height() == alpha[f]->height()), 
                    G3D::format("Texture images for RGB + R -> RGBA packing conversion must be the same size. (Loading %s + %s)",
                        filename.c_str(), alphaFilename.c_str()));
                /** write into new map byte-by-byte, copying over alpha properly */
                const int N = color[f]->height() * color[f]->width();
                const int colorStride = cbuf->format()->numComponents;
                const int alphaStride = abuf->format()->numComponents;
                for (int i = 0; i < N; ++i) {
                    newMap[i * 4 + 0] = colorMap[i * colorStride + 0];
                    newMap[i * 4 + 1] = colorMap[i * colorStride + 1];
                    newMap[i * 4 + 2] = colorMap[i * colorStride + 2];
                    newMap[i * 4 + 3] = useAlpha ? alphaMap[i * 4 + 3] : alphaMap[i * alphaStride];
                }
                cbuf->unmap();
                abuf->unmap();
            } else { // No color map, use white
                if (numFaces > 1) {
                    transform(alpha[f], alphaInfo.face[f]);
                }
                const shared_ptr<PixelTransferBuffer>& abuf = alpha[f]->toPixelTransferBuffer();
                const uint8* alphaMap                   = reinterpret_cast<const uint8*>(abuf->mapRead());

                /** write into new map byte-by-byte, copying over alpha properly */
                const int N = alpha[f]->height() * alpha[f]->width();
                const int alphaStride = abuf->format()->numComponents;
                for (int i = 0; i < N; ++i) {
                    newMap[i * 4 + 0] = 255;
                    newMap[i * 4 + 1] = 255;
                    newMap[i * 4 + 2] = 255;
                    newMap[i * 4 + 3] = useAlpha ? alphaMap[i * 4 + 3] : alphaMap[i * alphaStride];
                }
                abuf->unmap();
            }
            
            b->unmap();
            buffers[f] = b;
            array[f] = static_cast<uint8*>(b->buffer());
        }

        t = fromMemory
               (filename, 
                mip, 
                ImageFormat::SRGBA8(),
                buffers[0]->width(), 
                buffers[0]->height(), 
                1,
                1,
                desiredEncoding,
                dimension,
                generateMipMaps,
                preprocess,
                preferSRGBSpaceForAuto);

    } catch (const Image::Error& e) {
        Log::common()->printf("\n**************************\n\n"
            "Loading \"%s\" failed. %s\n", e.filename.c_str(),
            e.reason.c_str());
    }

    return t;
}
    

shared_ptr<Texture> Texture::fromMemory
   (const String&                       name,
    const Array< Array<const void*> >&  _bytes,
    const ImageFormat*                  bytesFormat,
    int                                 width,
    int                                 height,
    int                                 depth,
    int                                 numSamples,
    Encoding                            desiredEncoding,
    Dimension                           dimension,
    bool                                generateMipMaps,
    const Preprocess&                   preprocess,
    bool                                preferSRGBForAuto) {

    const shared_ptr<Texture>& t = createShared<Texture>(name, width, height, depth, dimension, desiredEncoding, numSamples, false);    
    s_allTextures.set((uintptr_t)t.get(), t);
    // Convert to PixelTransferBuffers using the same memory

    t->m_loadingInfo = new LoadingInfo(LoadingInfo::PREPROCESS);

    LoadingInfo& info = *t->m_loadingInfo;
    info.ptbArray.resize(_bytes.size());
    // for each mip
    for (int m = 0; m < _bytes.size(); ++m) {
        const Array<const void*>& src = _bytes[m];
        Array<shared_ptr<PixelTransferBuffer>>& dst = info.ptbArray[m];
        dst.resize(src.size());
        // for each cube face
        for (int f = 0; f < src.size(); ++f) {
            debugAssertM(notNull(src[f]), "Null pointer passed to Texture::fromMemory"); 
            // Allocate without copying data. This will automatically
            // be destroyed without freeing the caller's data when the array
            // leaves scope.
            dst[f] = CPUPixelTransferBuffer::fromData(width, height, bytesFormat, (void*)src[f], depth);
        }
    }

    info.desiredEncoding   = desiredEncoding;
    // Because the data are shared, we cannot lazy load
    info.lazyLoadable      = false;
    info.generateMipMaps   = generateMipMaps;
    info.preferSRGBForAuto = preferSRGBForAuto;

    t->completeCPULoading();
    t->completeGPULoading();

    return t;
}


shared_ptr<Texture> Texture::fromImage
   (const String&                   name,
    const shared_ptr<Image>&        image,
    const ImageFormat*              desiredFormat,
    Dimension                       dimension,
    bool                            generateMipMaps,
    const Preprocess&               preprocess) {
    return Texture::fromPixelTransferBuffer(name, image->toPixelTransferBuffer(), desiredFormat, dimension, generateMipMaps, preprocess);
}


shared_ptr<Texture> Texture::fromPixelTransferBuffer
   (const String&                   name,
    const shared_ptr<PixelTransferBuffer>& ptb,
    Encoding                        desiredEncoding,
    Dimension                       dimension,
    bool                            generateMipMaps,
    const Preprocess&               preprocess) {

    const shared_ptr<Texture>& t = createShared<Texture>(name, ptb->width(), ptb->height(), ptb->depth(), dimension, desiredEncoding, 1, false);    
    s_allTextures.set((uintptr_t)t.get(), t);
    // Convert to PixelTransferBuffers using the same memory

    t->m_loadingInfo = new LoadingInfo(LoadingInfo::PREPROCESS);

    LoadingInfo& info = *t->m_loadingInfo;
    info.ptbArray.resize(1);
    info.ptbArray[0].append(ptb);

    info.desiredEncoding   = desiredEncoding;
    // Because the data are shared, we cannot lazy load
    info.lazyLoadable      = false;
    info.generateMipMaps   = generateMipMaps;
    info.preprocess        = preprocess;

    t->completeCPULoading();
    t->completeGPULoading();

    return t;
}

shared_ptr<Texture> Texture::createEmpty
(const String&                    name,
 int                              width,
 int                              height,
 const Encoding&                  encoding,
 Dimension                        dimension,
 bool                             allocateMIPMaps,
 int                              depth,
 int                              numSamples) {

    debugAssertGLOk();
    debugAssertM(encoding.format, "encoding.format may not be ImageFormat::AUTO()");

    if ((dimension != DIM_3D) && (dimension != DIM_2D_ARRAY) && (dimension != DIM_CUBE_MAP_ARRAY)) {
        debugAssertM(depth == 1, "Depth must be 1 for DIM_2D textures");
    }

    debugAssert(notNull(encoding.format));
    
    // Check for at least one miplevel on the incoming data
    int maxRes = std::max(width, std::max(height, depth));
    int numMipMaps = allocateMIPMaps ? int(log2(float(maxRes)))+1 : 1;
    debugAssert(numMipMaps > 0);
    
    // Create the texture
    GLuint textureID = newGLTextureID();
    GLenum target = dimensionToTarget(dimension, numSamples);
        
    debugAssertM(GLCaps::supportsTexture(encoding.format), "Unsupported texture format.");

    int mipWidth   = width;
    int mipHeight  = height;
    int mipDepth   = depth;
    Color4 minval  = Color4::nan();
    Color4 meanval = Color4::nan();
    Color4 maxval  = Color4::nan();
    AlphaFilter alphaFilter = AlphaFilter::DETECT;

    glStatePush(); {

        glBindTexture(target, textureID);
        debugAssertGLOk();

        if (GLCaps::supports_glTexStorage2D() && ((target == GL_TEXTURE_2D) || (target == GL_TEXTURE_CUBE_MAP))) {
            glTexStorage2D(target, numMipMaps, encoding.format->openGLFormat, width, height);
        } else {
            for (int mipLevel = 0; mipLevel < numMipMaps; ++mipLevel) {
                int numFaces = (dimension == DIM_CUBE_MAP) ? 6 : 1;

                for (int f = 0; f < numFaces; ++f) {
                    if (numFaces == 6) {
                        // Choose the appropriate face target
                        target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + f;
                    }

                    debugAssertGLOk();
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
                    debugAssertGLOk();

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

                mipWidth  = G3D::max(1, mipWidth  / 2);
                mipHeight = G3D::max(1, mipHeight / 2);
                mipDepth  = G3D::max(1, mipDepth  / 2);            
            }
        }
   
    } glStatePop();

    debugAssertGLOk();
    const shared_ptr<Texture>& t = fromGLTexture(name, textureID, encoding.format, alphaFilter, dimension);
    debugAssertGLOk();

    t->m_width  = width;
    t->m_height = height;
    t->m_depth  = depth;
    t->m_min    = minval;
    t->m_max    = maxval;
    t->m_mean   = meanval;
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

    debugAssertGLOk();
    return t;
}


void Texture::resize(int w, int h) {
    force();
    if ((width() == w) && (height() == h)) {
        return;
    }

    // Call reallocation hook
    reallocateHook(m_textureID);

    m_sizeOfAllTexturesInMemory -= sizeInMemory();
    
    alwaysAssertM(m_dimension != DIM_CUBE_MAP , "Cannot resize cube map textures");
    Array<GLenum> targets;
    if (m_dimension == DIM_CUBE_MAP) {
        targets.append(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
                        GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
                        GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB);
        targets.append(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
                        GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
                        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB);        
    } else {
        targets.append(openGLTextureTarget());
    }
    debugAssertGLOk();

    glStatePush();
    {
        glBindTexture(openGLTextureTarget(), m_textureID);
        debugAssertGLOk();
        const int numMipMaps = m_hasMipMaps ? int(log2(float(std::max(w, h)))) + 1 : 1;
        for (int t = 0; t < targets.size(); ++t) {
            if (targets[t] == GL_TEXTURE_2D_MULTISAMPLE){
                glTexImage2DMultisample(targets[t], m_numSamples, format()->openGLFormat, w, h, false);
            } else if (GLCaps::supports_glTexStorage2D() && ((openGLTextureTarget() == GL_TEXTURE_2D) || (openGLTextureTarget() == GL_TEXTURE_CUBE_MAP))) {
                // Use the new GL 4.2 call for all MIP levels
                glTexStorage2D(openGLTextureTarget(), numMipMaps, format()->openGLFormat, w, h);                
                debugAssertGLOk();
            } else {
                int mipWidth = w, mipHeight = h;
                for (int i = 0; i < numMipMaps; ++i, mipWidth /= 2, mipHeight /= 2) {
                    glTexImage2D(targets[t], i, format()->openGLFormat, std::max(1, mipWidth), std::max(1, mipHeight), 0, format()->openGLBaseFormat, format()->openGLDataFormat, nullptr);
                    debugAssertGLOk();
                }
            }
        }
    }
    glStatePop();

    m_min  = Color4::nan();
    m_max  = Color4::nan();
    m_mean = Color4::nan();

    m_width = w;
    m_height = h;
    m_depth = 1;

    m_sizeOfAllTexturesInMemory += sizeInMemory();

    debugAssertGLOk();
}


void Texture::resize(int w, int h, int d) {

    force();
    if (d == 1) {
        // 2D case
        resize(w, h);
    } else if (m_width != w || m_height != h || m_depth != d) {
        reallocateHook(m_textureID);
        m_width = w;
        m_height = h;
        m_depth = d;

        alwaysAssertM(m_dimension != DIM_CUBE_MAP, "Cannot resize cube map textures");
        Array<GLenum> targets;
        if (m_dimension == DIM_CUBE_MAP) {
            targets.append(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
                           GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB);
            targets.append(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
                           GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB);        
        } else {
            targets.append(openGLTextureTarget());
        }

        glStatePush();
        {
            glBindTexture(openGLTextureTarget(), m_textureID);
            for (int t = 0; t < targets.size(); ++t) {
                glTexImage3D(targets[t], 0, format()->openGLFormat, w, h, d,
                                     0, format()->openGLBaseFormat, GL_UNSIGNED_BYTE, NULL);
            }
        }
        glStatePop();

        m_sizeOfAllTexturesInMemory += sizeInMemory();
    }

    debugAssertGLOk();
}


static bool isSRGBFormat(const ImageFormat* format) {
    return format->colorSpace == ImageFormat::COLOR_SPACE_SRGB;
}

void Texture::copy
   (shared_ptr<Texture>     src, 
    shared_ptr<Texture>     dst, 
    int                     srcMipLevel, 
    int                     dstMipLevel, 
    float                   scale,
    const Vector2int16&     shift,
    CubeFace                srcCubeFace, 
    CubeFace                dstCubeFace, 
    RenderDevice*           rd,
    bool                    resize,
    int                     srcLayer,
    int                     dstLayer) {

    alwaysAssertM((src->format()->depthBits == 0) || (srcMipLevel == 0 && dstMipLevel == 0), 
            "Texture::copy only defined for mipLevel 0 for depth textures");
    alwaysAssertM((src->format()->depthBits == 0) == (dst->format()->depthBits == 0), "Cannot copy color texture to depth texture or vice-versa");
    alwaysAssertM( ((src->dimension() == DIM_2D) || (src->dimension() == DIM_2D_ARRAY)), "Texture::copy only defined for 2D textures or texture arrays");
    //alwaysAssertM(((dst->dimension() == DIM_2D) || (dst->dimension() == DIM_2D_ARRAY)), "Texture::copy only defined for 2D textures or texture arrays");
    alwaysAssertM((dst->dimension() == DIM_2D_ARRAY) || (dstLayer == 0), "Layer can only be 0 for non-array textures");
    alwaysAssertM((src->dimension() == DIM_2D_ARRAY) || (srcLayer == 0), "Layer can only be 0 for non-array textures");

    alwaysAssertM( src && dst, "Both textures sent to Texture::copy must already exist");
    
    if (resize) {
        if (srcMipLevel != dstMipLevel) {
            alwaysAssertM(dstMipLevel == 0, "If miplevels mismatch, dstMipLevel must be 0 in Texture::copy");
            const int mipFactor = 1 << srcMipLevel;
            dst->resize(int(src->width() / (mipFactor * scale)), int(src->height() * scale / mipFactor));
        } else {
            dst->resize(int(src->width() / scale), int(src->height() * scale));
        }
    }

    const shared_ptr<Framebuffer>& fbo = workingFramebuffer();
    if (isNull(rd)) {
        rd = RenderDevice::current;
    }
    fbo->clear();

    // Is there a fast path?
    if (! isSRGBFormat(src->format()) && ! isSRGBFormat(dst->format()) && (dstMipLevel == srcMipLevel) &&
        (scale == 1.0f) && shift.isZero() && ! dst->isCubeMap() && (dstLayer == 0)) { 
        if (src->format()->depthBits > 0) {
            fbo->set(Framebuffer::DEPTH, src, srcCubeFace, srcMipLevel, (src->dimension() != DIM_2D_ARRAY) ? -1 : srcLayer);
        } else {
            fbo->set(Framebuffer::COLOR0, src, srcCubeFace, srcMipLevel, (src->dimension() != DIM_2D_ARRAY) ? -1 : srcLayer);
        }
        rd->pushState(fbo); {
            const GLenum target = dst->openGLTextureTarget();
            glBindTexture(target, dst->openGLID());

            if (target == GL_TEXTURE_2D_ARRAY) {
                glCopyTexSubImage3D(target, dstMipLevel, 0, 0, dstLayer, 0, 0, dst->width(), dst->height());
            } else {
                glCopyTexSubImage2D(target, dstMipLevel, 0, 0, 0, 0, dst->width(), dst->height());
            }
            debugAssertGLOk();
            glBindTexture(dst->openGLTextureTarget(), GL_NONE);
        } rd->popState();
        return;
    }


    /** If it isn't an array texture, then don't try to bind a single layer */
    if ((dst->dimension() != DIM_2D_ARRAY) && (dst->dimension() != DIM_CUBE_MAP_ARRAY)) {
        dstLayer = -1;
    }
    if (src->format()->depthBits > 0) {
        fbo->set(Framebuffer::DEPTH, dst, dstCubeFace, dstMipLevel, dstLayer);
    } else {
        fbo->set(Framebuffer::COLOR0, dst, dstCubeFace, dstMipLevel, dstLayer);
    }
    
    rd->push2D(fbo); {
        rd->setSRGBConversion(true);
        if (src->format()->depthBits > 0) {
            rd->setDepthClearValue(1.0f);
            rd->setDepthWrite(true);
        } else {
            rd->setColorClearValue(Color4::zero());
        }
        rd->clear();
        
        Args args;
        args.setUniform("mipLevel", srcMipLevel);
        
        const bool layered = (src->dimension() == Texture::DIM_2D_ARRAY);
        args.setMacro("IS_LAYERED", layered ? 1 : 0);
        args.setUniform("layer",    srcLayer);
        args.setUniform("src", layered ? Texture::zero() : src, Sampler::video());
        args.setUniform("layeredSrc", layered ? src : Texture::zero(Texture::DIM_2D_ARRAY), Sampler::video());
        
        args.setUniform("shift",    Vector2(shift));
        args.setUniform("scale",    scale);
        args.setMacro("DEPTH",      (src->format()->depthBits > 0) ? 1 : 0);
        args.setRect(rd->viewport());

        LAUNCH_SHADER_WITH_HINT("Texture_copy.*", args, src->name() + "->" + dst->name());
    } rd->pop2D();

    fbo->clear();
}


bool Texture::copyInto(shared_ptr<Texture>& dest, CubeFace cf, int mipLevel, RenderDevice* rd) const {
    alwaysAssertM
        (((format()->depthBits == 0) || mipLevel == 0) && ((dimension() == DIM_2D) || (dimension() == DIM_2D)), 
        "copyInto only defined for 2D color textures as input, or mipLevel 0 of a depth texture");

    bool allocated = false;
    if (isNull(dest) || (dest->format() != format())) {
        // Allocate
        dest = Texture::createEmpty(name() + " copy", width(), height(), format(), dimension(), hasMipMaps(), depth());
        allocated = true;
    }

    dest->resize(width(), height());

    if (isNull(rd)) {
        rd = RenderDevice::current;
    }

#if 0 // Optimized fast path...not working
    const shared_ptr<Framebuffer>& dstFBO = workingFramebuffer();
    const shared_ptr<Framebuffer>& srcFBO = workingFramebuffer2();

    srcFBO->clear();
    dstFBO->clear();
    const shared_ptr<Texture>& src = dynamic_pointer_cast<Texture>(const_cast<Texture*>(this)->shared_from_this());
    const bool isDepth = format()->depthBits > 0;
    if (isDepth) {
        srcFBO->set(Framebuffer::DEPTH, src, cf, mipLevel);
        dstFBO->set(Framebuffer::DEPTH, dest, cf, mipLevel);
    } else {
        srcFBO->set(Framebuffer::COLOR0, src, cf, mipLevel);
        dstFBO->set(Framebuffer::COLOR0, dest, cf, mipLevel);
    }
    srcFBO->blitTo(rd, dstFBO, false, false, isDepth, format()->stencilBits > 0, ! isDepth); 
    srcFBO->clear();
    dstFBO->clear();
#else
    const shared_ptr<Framebuffer>& fbo = workingFramebuffer();

    fbo->clear();
    if (format()->depthBits > 0) {
        fbo->set(Framebuffer::DEPTH, dest, cf, mipLevel);
    } else {
        fbo->set(Framebuffer::COLOR0, dest, cf, mipLevel);
    }
    
    rd->push2D(fbo); {
        if (format()->depthBits > 0) {
            rd->setDepthClearValue(1.0f);
            rd->setDepthWrite(true);
        } else {
            rd->setColorClearValue(Color4::zero());
        }
        rd->clear();
        rd->setSRGBConversion(true);
        Args args;
        args.setUniform("mipLevel", mipLevel);
        const shared_ptr<Texture>& me = dynamic_pointer_cast<Texture>(const_cast<Texture*>(this)->shared_from_this());
        args.setUniform("src",      me, Sampler::buffer());

        bool layered = (me->dimension() == Texture::DIM_2D_ARRAY);
        args.setMacro("IS_LAYERED", layered ? 1 : 0);
        args.setUniform("layer", 0);
        args.setUniform("src", layered ? Texture::zero() : me, Sampler::video());
        args.setUniform("layeredSrc", layered ? me : Texture::zero(Texture::DIM_2D_ARRAY), Sampler::video());

        args.setUniform("shift",    Vector2(0, 0));
        args.setUniform("scale",    1.0f);
        args.setMacro("DEPTH", (format()->depthBits > 0) ? 1 : 0);
        args.setRect(rd->viewport());

        LAUNCH_SHADER_WITH_HINT("Texture_copy.*", args, name());
    } rd->pop2D();

    fbo->clear();
#endif
    return allocated;
}


void Texture::clear(int mipLevel) {
    force();
#   ifdef G3D_OSX
    {
        RenderDevice* rd = RenderDevice::current;
        
        const shared_ptr<Framebuffer>& fbo = workingFramebuffer();
        
        const int numCubeFaces = isCubeMap() ? 6 : 1;
        // Doesn't currently support tracking the maximum mipmap
        const int numMipLevels = m_hasMipMaps ? 1 : 1;
        
        const shared_ptr<Texture>& ptr = dynamic_pointer_cast<Texture>(shared_from_this());
        for (int mipLevel = 0; mipLevel < numMipLevels; ++mipLevel) {
            for (int cf = 0; cf < numCubeFaces; ++cf) {
                fbo->set((format()->depthBits > 0) ? Framebuffer::DEPTH : Framebuffer::COLOR0, ptr, (CubeFace)cf, mipLevel);
                rd->pushState(fbo);    
                rd->clear();
                rd->popState();
            }
        }
        fbo->clear();
    }
#   else
    {
        glClearTexImage(m_textureID, mipLevel, format()->openGLBaseFormat, format()->openGLDataFormat, nullptr);
    }
#   endif
    debugAssertGLOk();
}


Rect2D Texture::rect2DBounds() const {
    return Rect2D::xywh(0, 0, float(m_width), float(m_height));
}


void Texture::getTexImage(void* data, const ImageFormat* desiredFormat, CubeFace face, int mipLevel) const {
    force();
    const shared_ptr<GLPixelTransferBuffer>& transferBuffer = toPixelTransferBuffer(desiredFormat, mipLevel, face);
    transferBuffer->getData(data);
}


shared_ptr<Image4> Texture::toImage4() const {
    const shared_ptr<Image4>& im = Image4::createEmpty(m_width, m_height, WrapMode::TILE, m_depth); 
    getTexImage(im->getCArray(), ImageFormat::RGBA32F());

    if ((m_encoding.format->openGLBaseFormat == GL_LUMINANCE) || 
        (m_encoding.format->openGLBaseFormat == GL_LUMINANCE_ALPHA)) {
        // Spread the R values across the G and B channels, since getTexImage 
        // doesn't automatically do that
        Color4* ptr = im->getCArray();
        const int N = im->width() * im->height();
        for (int i = 0; i < N; ++i, ++ptr) {
            ptr->g = ptr->b = ptr->r;
        }
    }

    return im;
}


shared_ptr<Image3> Texture::toImage3(CubeFace face, int mip) const {    
    const shared_ptr<Image3>& im = Image3::createEmpty(m_width, m_height, WrapMode::TILE, m_depth); 
    getTexImage(im->getCArray(), ImageFormat::RGB32F(), face, mip);
    
    if (format()->numComponents == 1) {
        // Convert R -> RGB
        Color3* ptr = im->getCArray();
        const int N = im->width() * im->height();
        runConcurrently(0, N, [&](int i) {
            Color3& c = ptr[i];
            c.g = c.b = c.r;
        });
    }

    return im;
}


shared_ptr<Map2D<float> > Texture::toDepthMap() const {
    const shared_ptr<Map2D<float> >& im = Map2D<float>::create(m_width, m_height, WrapMode::TILE); 
    getTexImage(im->getCArray(), ImageFormat::DEPTH32F());
    return im;
}


shared_ptr<Image1> Texture::toDepthImage1() const {
    shared_ptr<Image1> im = Image1::createEmpty(m_width, m_height, WrapMode::TILE);
    getTexImage(im->getCArray(), ImageFormat::DEPTH32F());
    return im;
}


shared_ptr<Image1unorm8> Texture::toDepthImage1unorm8() const {
    shared_ptr<Image1> src = toDepthImage1();
    shared_ptr<Image1unorm8> dst = Image1unorm8::createEmpty(m_width, m_height, WrapMode::TILE);
    
    const Color1* s = src->getCArray();
    Color1unorm8* d = dst->getCArray();
    
    // Float to int conversion
    for (int i = m_width * m_height - 1; i >= 0; --i) {
        d[i] = Color1unorm8(s[i]);
    }
    
    return dst;
}


shared_ptr<Image1> Texture::toImage1() const {
    const shared_ptr<Image1>& im = Image1::createEmpty(m_width, m_height, WrapMode::TILE); 
    getTexImage(im->getCArray(), ImageFormat::L32F());
    return im;
}


shared_ptr<CubeMap> Texture::toCubeMap() const {
    Array<shared_ptr<Image3>> faceImage;

    faceImage.resize(6);
    for (int f = 0; f < 6; ++f) {
        faceImage[f] = toImage3(CubeFace(f), 0);
    }

    return CubeMap::create(faceImage, m_encoding.readMultiplyFirst.rgb(), m_encoding.readAddSecond.rgb());
}


void Texture::splitFilenameAtWildCard
   (const String&  filename,
    String&        filenameBase,
    String&        filenameExt) {

    const String splitter = "*";

    size_t i = filename.rfind(splitter);
    if (i != String::npos) {
        filenameBase = filename.substr(0, i);
        filenameExt  = filename.substr(i + 1, filename.size() - i - splitter.length()); 
    } else {
        throw Image::Error("Cube map filenames must contain \"*\" as a "
            "placeholder for {up,lf,rt,bk,ft,dn} or {up,north,south,east,west,down}", filename);
    }
}


bool Texture::isSupportedImage(const String& filename) {
    // Reminder: this looks in zipfiles as well
    if (! FileSystem::exists(filename)) {
        return false;
    }

    String ext = toLower(filenameExt(filename));

    if ((ext == "jpg") ||    
        (ext == "ico") ||
        (ext == "dds") ||
        (ext == "png") ||
        (ext == "tga") || 
        (ext == "bmp") ||
        (ext == "ppm") ||
        (ext == "pgm") ||
        (ext == "pbm") ||
        (ext == "tiff") ||
        (ext == "exr") ||
        (ext == "cut") ||
        (ext == "psd") ||
        (ext == "jbig") ||
        (ext == "xbm") ||
        (ext == "xpm") ||
        (ext == "gif") ||
        (ext == "hdr") ||
        (ext == "iff") ||
        (ext == "jng") ||
        (ext == "pict") ||
        (ext == "ras") ||
        (ext == "wbmp") ||
        (ext == "sgi") ||
        (ext == "pcd") ||
        (ext == "jp2") ||
        (ext == "jpx") ||
        (ext == "jpf") ||
        (ext == "pcx")) {
        return true;
    } else {
        return false;
    } 
}


Texture::~Texture() {
    reallocateHook(m_textureID);
    s_allTextures.remove((uintptr_t)this);
    if (m_destroyGLTextureInDestructor) {
        if (notNull(m_loadingThread)) {
            m_loadingThread->detach();
            delete m_loadingThread;
            m_loadingThread = nullptr;
        }

        m_sizeOfAllTexturesInMemory -= sizeInMemory();
        if (m_textureID != GL_NONE) {
            glDeleteTextures(1, &m_textureID);
        }
        m_textureID = 0;
    }
}


unsigned int Texture::newGLTextureID() {
    // Clear the OpenGL error flag
#   ifdef G3D_DEBUG
        glGetError();
#   endif 

    unsigned int id;
    glGenTextures(1, &id);

    debugAssertM(glGetError() != GL_INVALID_OPERATION, 
         "GL_INVALID_OPERATION: Probably caused by invoking "
         "glGenTextures between glBegin and glEnd.");

    return id;
}


void Texture::copyFromScreen(const Rect2D& rect, const ImageFormat* fmt) {
    force();
    glStatePush();
    debugAssertGLOk();

    m_sizeOfAllTexturesInMemory -= sizeInMemory();

    if (isNull(fmt)) {
        fmt = format();
    } else {
        m_encoding = Encoding(fmt);
    }

    // Set up new state
    m_width   = (int)rect.width();
    m_height  = (int)rect.height();
    m_depth   = 1;
    debugAssert(m_dimension == DIM_2D || m_dimension == DIM_2D_RECT || m_dimension == DIM_2D);

    const GLenum target = dimensionToTarget(m_dimension, m_numSamples);

    debugAssertGLOk();
    glBindTexture(target, m_textureID);
    debugAssertGLOk();

    glCopyTexImage2D(target, 0, format()->openGLFormat,
                     iRound(rect.x0()), 
                     iRound(rect.y0()), 
                     iRound(rect.width()), 
                     iRound(rect.height()), 
                     0);

    debugAssertGLOk();
    // Reset the original properties
    setAllSamplerParameters(target, m_cachedSamplerSettings);

    debugAssertGLOk();

    glStatePop();

    m_sizeOfAllTexturesInMemory += sizeInMemory();
}



void Texture::copyFromScreen(
    const Rect2D&       rect,
    CubeFace            face) {
    force();

    glStatePush();

    // Set up new state
    debugAssertM(m_width == rect.width(), "Cube maps require all six faces to have the same dimensions");
    debugAssertM(m_height == rect.height(), "Cube maps require all six faces to have the same dimensions");
    debugAssert(m_dimension == DIM_CUBE_MAP);

    if (GLCaps::supports_GL_ARB_multitexture()) {
        glActiveTextureARB(GL_TEXTURE0_ARB);
    }
    glDisableAllTextures();

    glEnable(GL_TEXTURE_CUBE_MAP_ARB);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, m_textureID);

    GLenum target = openGLTextureTarget();
    if (isCubeMap()) {
        target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face;
    }

    debugAssertGLOk();

    double viewport[4];
    glGetDoublev(GL_VIEWPORT, viewport);
    double viewportHeight = viewport[3];
    debugAssertGLOk();

    glCopyTexImage2D(target, 0, format()->openGLFormat,
                     iRound(rect.x0()), 
                     iRound(viewportHeight - rect.y1()), 
                     iRound(rect.width()), 
                     iRound(rect.height()), 0);

    debugAssertGLOk();
    glDisable(GL_TEXTURE_CUBE_MAP_ARB);
    glStatePop();
}


void Texture::getCubeMapRotation(CubeFace face, Matrix3& outMatrix) {
    switch (face) {
    case CubeFace::POS_X:
        outMatrix = Matrix3::fromAxisAngle(Vector3::unitY(), (float)-halfPi());
        break;
        
    case CubeFace::NEG_X:
        outMatrix = Matrix3::fromAxisAngle(Vector3::unitY(), (float)halfPi());
        break;
        
    case CubeFace::POS_Y:
       outMatrix = CFrame::fromXYZYPRDegrees(0,0,0,0,90,0).rotation;
        break;
        
    case CubeFace::NEG_Y: 
        outMatrix = CFrame::fromXYZYPRDegrees(0,0,0,0,-90,0).rotation;
        break;
        
    case CubeFace::POS_Z:
        outMatrix = Matrix3::identity();
        break;
        
    case CubeFace::NEG_Z:
        outMatrix = Matrix3::fromAxisAngle(Vector3::unitY(), (float)pi());
        break;

    default:
        alwaysAssertM(false, "");
    }
    
    // GL's cube maps are "inside out" (they are the outside of a box,
    // not the inside), but its textures are also upside down, so
    // these turn into a 180-degree rotation, which fortunately does
    // not affect the winding direction.
    outMatrix = Matrix3::fromAxisAngle(Vector3::unitZ(), toRadians(180)) * -outMatrix;
}


int Texture::sizeInMemory() const {

    int64 base = (m_width * m_height * m_depth * m_encoding.format->openGLBitsPerPixel) / 8;

    int64 total = 0;

    if (m_hasMipMaps) {
        int w = m_width;
        int h = m_height;

        while ((w > 2) && (h > 2)) {
            total += base;
            base /= 4;
            w /= 2;
            h /= 2;
        }

    } else {
        total = base;
    }

    if (m_dimension == DIM_CUBE_MAP) {
        total *= 6;
    }

    return (int)total;
}


unsigned int Texture::openGLTextureTarget() const {
   return dimensionToTarget(m_dimension, m_numSamples);
}


shared_ptr<Texture> Texture::alphaOnlyVersion() const {
    force();
    if (opaque()) {
        return nullptr;
    }
    debugAssertM(
        m_dimension == DIM_2D ||
        m_dimension == DIM_2D_RECT ||
        m_dimension == DIM_2D,
        "alphaOnlyVersion only supported for 2D textures");
    
    int numFaces = 1;

    Array< Array<const void*> > mip;
    mip.resize(1);
    Array<const void*>& bytes = mip[0];
    bytes.resize(numFaces);
    const ImageFormat* bytesFormat = ImageFormat::A8();

    glStatePush();
    // Setup to later implement cube faces
    for (int f = 0; f < numFaces; ++f) {
        GLenum target = dimensionToTarget(m_dimension, m_numSamples);
        glBindTexture(target, m_textureID);
        bytes[f] = (const void*)System::malloc(m_width * m_height);
        glGetTexImage(target, 0, GL_ALPHA, GL_UNSIGNED_BYTE, const_cast<void*>(bytes[f]));
    }

    glStatePop();
    const int numSamples = 1;
    const shared_ptr<Texture>& ret = 
        fromMemory(
            m_name + " Alpha", 
            mip,
            bytesFormat,
            m_width, 
            m_height, 
            1, 
            numSamples,
            ImageFormat::A8(),
            m_dimension);

    for (int f = 0; f < numFaces; ++f) {
        System::free(const_cast<void*>(bytes[f]));
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////////

void Texture::setDepthTexParameters(GLenum target, DepthReadMode depthReadMode) {
    debugAssertGLOk();
    //glTexParameteri(target, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);

    if (depthReadMode == DepthReadMode::DEPTH_NORMAL) {
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    } else {
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, 
                        (depthReadMode == DepthReadMode::DEPTH_LEQUAL) ? GL_LEQUAL : GL_GEQUAL);
    }

    debugAssertGLOk();
}


static void setWrapMode(GLenum target, WrapMode wrapMode) {
    GLenum mode = GL_NONE;
    
    switch (wrapMode) {
    case WrapMode::TILE:
        mode = GL_REPEAT;
        break;

    case WrapMode::CLAMP:  
        if (target != GL_TEXTURE_2D_MULTISAMPLE) {
            mode = GL_CLAMP_TO_EDGE;
        } else {
            mode = GL_CLAMP;
        }
        break;

    case WrapMode::ZERO:
        mode = GL_CLAMP_TO_BORDER;
        glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, reinterpret_cast<const float*>(&Color4::clear()));
        debugAssertGLOk();
        break;

    default:
        debugAssertM(Texture::supportsWrapMode(wrapMode), "Unsupported wrap mode for Texture");
    }
    debugAssertGLOk();


    if (target != GL_TEXTURE_2D_MULTISAMPLE) {
        glTexParameteri(target, GL_TEXTURE_WRAP_S, mode);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, mode);
        glTexParameteri(target, GL_TEXTURE_WRAP_R, mode);

        debugAssertGLOk();
    }
}


static bool textureHasMipMaps(GLenum target, InterpolateMode interpolateMode) {
    return (target != GL_TEXTURE_RECTANGLE) &&
        (interpolateMode != InterpolateMode::BILINEAR_NO_MIPMAP) &&
        (interpolateMode != InterpolateMode::NEAREST_NO_MIPMAP) &&
        (target != GL_TEXTURE_2D_MULTISAMPLE);}


static void setMinMaxMipMaps(GLenum target, bool hasMipMaps, int minMipMap, int maxMipMap) {
    if (hasMipMaps) {
        glTexParameteri(target, GL_TEXTURE_MAX_LOD_SGIS, maxMipMap);
        glTexParameteri(target, GL_TEXTURE_MIN_LOD_SGIS, minMipMap);

        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, maxMipMap);
        
    }
}


static void setInterpolateMode(GLenum target, InterpolateMode interpolateMode) {
    if (target != GL_TEXTURE_2D_MULTISAMPLE) {
        switch (interpolateMode) {
        case InterpolateMode::TRILINEAR_MIPMAP:
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            break;

        case InterpolateMode::BILINEAR_MIPMAP:
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            break;
            
        case InterpolateMode::NEAREST_MIPMAP:
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            break;
            
        case InterpolateMode::BILINEAR_NO_MIPMAP:
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
            break;
            
        case InterpolateMode::NEAREST_NO_MIPMAP:
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            break;
            
        default:
            debugAssert(false);
        }
        debugAssertGLOk();
    }
}
    

static void setMaxAnisotropy(GLenum target, bool hasMipMaps, float maxAnisotropy) {
    static const bool anisotropic = GLCaps::supports("GL_EXT_texture_filter_anisotropic");

    if (anisotropic && hasMipMaps) {
        glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
    }
}


static void setMipBias(GLenum target, float mipBias) {
    if (mipBias != 0.0f) {
        glTexParameterf(target, GL_TEXTURE_LOD_BIAS, mipBias);
    }
}


void Texture::setDepthReadMode(GLenum target, DepthReadMode depthReadMode) {
    if (target != GL_TEXTURE_2D_MULTISAMPLE) {
        Texture::setDepthTexParameters(target, depthReadMode);
        debugAssertGLOk();
    }
}


void Texture::setAllSamplerParameters
    (GLenum               target,
    const Sampler&        settings) {
    
    debugAssert(
        target == GL_TEXTURE_2D || target == GL_TEXTURE_2D_MULTISAMPLE ||
        target == GL_TEXTURE_RECTANGLE_EXT ||
        target == GL_TEXTURE_CUBE_MAP ||
        target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_3D || 
        target == GL_TEXTURE_CUBE_MAP_ARRAY);

    debugAssertGLOk();
    
    const bool hasMipMaps = textureHasMipMaps(target, settings.interpolateMode);

    setWrapMode(target, settings.xWrapMode);
    debugAssertGLOk();
    setMinMaxMipMaps(target, hasMipMaps, settings.minMipMap, settings.maxMipMap);
    debugAssertGLOk();
    setInterpolateMode(target, settings.interpolateMode);
    debugAssertGLOk();
    setMaxAnisotropy(target, hasMipMaps, settings.maxAnisotropy);
    debugAssertGLOk();
    setMipBias(target, settings.mipBias);
    debugAssertGLOk();
    setDepthReadMode(target, settings.depthReadMode);
    debugAssertGLOk();
}


void Texture::updateSamplerParameters(const Sampler& settings) {
    force();
    GLenum target = dimensionToTarget(m_dimension, m_numSamples);
    debugAssert(
        target == GL_TEXTURE_2D || target == GL_TEXTURE_2D_MULTISAMPLE ||
        target == GL_TEXTURE_RECTANGLE_EXT ||
        target == GL_TEXTURE_CUBE_MAP ||
        target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_3D ||
        target == GL_TEXTURE_CUBE_MAP_ARRAY);

    debugAssertGLOk();

    bool hasMipMaps = textureHasMipMaps(target, settings.interpolateMode);
    
    if (settings.xWrapMode != m_cachedSamplerSettings.xWrapMode) {
        //debugAssertM(false, G3D::format("WrapMode: %s != %s", settings.wrapMode.toString(), m_cachedSamplerSettings.wrapMode.toString()));
        setWrapMode(target, settings.xWrapMode);
    }

    if (settings.minMipMap != m_cachedSamplerSettings.minMipMap ||
        settings.maxMipMap != m_cachedSamplerSettings.maxMipMap) {
        //debugAssertM(false, G3D::format("MinMipMap: %d != %d or MaxMipMap %d != %d", settings.minMipMap, m_cachedSamplerSettings.minMipMap, settings.maxMipMap, m_cachedSamplerSettings.maxMipMap));
        setMinMaxMipMaps(target, hasMipMaps, settings.minMipMap, settings.maxMipMap);
    }

    if (settings.interpolateMode != m_cachedSamplerSettings.interpolateMode) {
        //debugAssertM(false, G3D::format("InterpolateMode: %d != %d", settings.interpolateMode, m_cachedSamplerSettings.interpolateMode));
        setInterpolateMode(target, settings.interpolateMode);
    }

    if (settings.maxAnisotropy != m_cachedSamplerSettings.maxAnisotropy) {
        //debugAssertM(false, G3D::format("MaxAnisotropy: %f != %f", settings.maxAnisotropy, m_cachedSamplerSettings.maxAnisotropy));
        setMaxAnisotropy(target, hasMipMaps, settings.maxAnisotropy);
    }

    if (settings.mipBias != m_cachedSamplerSettings.mipBias) {
        //debugAssertM(false, G3D::format("MipBias: %f != %f", settings.mipBias, m_cachedSamplerSettings.mipBias));
        setMipBias(target, settings.mipBias);
    }

    if (settings.depthReadMode != m_cachedSamplerSettings.depthReadMode) {
        //debugAssertM(false, G3D::format("DepthReadMode: %f != %f", settings.depthReadMode, m_cachedSamplerSettings.depthReadMode));
        setDepthReadMode(target, settings.depthReadMode);
    }
    
    m_cachedSamplerSettings = settings;
}


static GLenum dimensionToTarget(Texture::Dimension d, int numSamples) {
    switch (d) {
    case Texture::DIM_CUBE_MAP:
        return GL_TEXTURE_CUBE_MAP;

    case Texture::DIM_CUBE_MAP_ARRAY:
        return GL_TEXTURE_CUBE_MAP_ARRAY;
        
    case Texture::DIM_2D:
        if (numSamples < 2) {
            return GL_TEXTURE_2D;
        } else {
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
        return 0 ;//GL_TEXTURE_2D;
    }
}


void computeStats
(const shared_ptr<PixelTransferBuffer>& ptb, 
 Color4&      minval,
 Color4&      maxval,
 Color4&      meanval,
 AlphaFilter& alphaFilter,
 const Texture::Encoding& encoding) {

    GLenum       bytesActualFormat = ptb->format()->openGLFormat;
    
    minval  = Color4::nan();
    maxval  = Color4::nan();
    meanval = Color4::nan();
    alphaFilter = AlphaFilter::DETECT;

    if (isNull(ptb)) {
        return;
    }

    const int width = ptb->width();
    const int height = ptb->height();
     
    const uint8* rawBytes = reinterpret_cast<const uint8*>(ptb->mapRead());
    debugAssert(rawBytes != nullptr);

    // For sRGB conversion
    const int UNINITIALIZED = 255;
    static int toRGB[255] = {UNINITIALIZED};
    if (toRGB[0] == UNINITIALIZED) {
        // Initialize
        for (int i = 0; i < 255; ++i) {
            toRGB[i] = iRound(pow(i / 255.0f, 2.15f) * 255.0f);
        }
    }

    const float inv255Width = 1.0f / (width * 255);
    switch (bytesActualFormat) {
    case GL_R8:
        {
            Array<float> rowSum;
            Array<unorm8> rowMin, rowMax;
            rowSum.resize(height);
            rowMin.resize(height);
            rowMax.resize(height);
            // Compute mean along rows to avoid overflow, and process all rows in parallel
            runConcurrently(0, height, [&](int y) {
                const unorm8* ptr = ((const unorm8*)rawBytes) + (y * width);
                unorm8 mn = unorm8::fromBits(255);
                unorm8 mx = unorm8::fromBits(0);
                int r = 0;
                for (int x = 0; x < width; ++x) {
                    const unorm8 i = ptr[x];
                    mn = min(mn, i);
                    mx = max(mx, i);
                    r += i.bits();
                }
                rowSum[y] = float(r) * inv255Width;
                rowMin[y] = mn;
                rowMax[y] = mx;
            });

            // Accumulate the individual row values
            unorm8 mn = unorm8::fromBits(255);
            unorm8 mx = unorm8::fromBits(0);
            meanval = Color4::zero();
            for (int y = 0; y < height; ++y) {
                meanval.r += float(rowSum[y]);
                mn = min(mn, rowMin[y]);
                mx = max(mx, rowMax[y]);
            }
            minval  = Color4(mn, 0, 0, 1.0f);
            maxval  = Color4(mx, 0, 0, 1.0f);
            meanval.r /= (float)height;
            meanval.a = 1.0f;
            alphaFilter = AlphaFilter::ONE;
        }
        break;

    case GL_RGB8:
        {
            Array<Color3> rowSum;
            Array<Color3unorm8> rowMin, rowMax;
            rowSum.resize(height);
            rowMin.resize(height);
            rowMax.resize(height);
            // Compute mean along rows to avoid overflow, and process all rows in parallel
            runConcurrently(0, height, [&](int y) {
                const Color3unorm8* ptr = ((const Color3unorm8*)rawBytes) + (y * width);
                Color3unorm8 mn = Color3unorm8::one();
                Color3unorm8 mx = Color3unorm8::zero();
                uint32 r = 0, g = 0, b = 0;
                for (int x = 0; x < width; ++x) {
                    const Color3unorm8 i = ptr[x];
                    mn = mn.min(i);
                    mx = mx.max(i);
                    r += i.r.bits(); g += i.g.bits(); b += i.b.bits();
                }
                rowSum[y] = Color3(float(r) * inv255Width, float(g) * inv255Width, float(b) * inv255Width);
                rowMin[y] = mn;
                rowMax[y] = mx;
            });

            // Accumulate the individual row values
            Color3unorm8 mn = Color3unorm8::one();
            Color3unorm8 mx = Color3unorm8::zero();
            meanval = Color4::zero();
            for (int y = 0; y < height; ++y) {
                meanval += Color4(rowSum[y], 0.0f);
                mn = mn.min(rowMin[y]);
                mx = mx.max(rowMax[y]);
            }
            minval  = Color4(Color3(mn), 1.0f);
            maxval  = Color4(Color3(mx), 1.0f);
            meanval /= (float)height;
            meanval.a = 1.0f;
            alphaFilter = AlphaFilter::ONE;
        }
        break;

    case GL_RGBA8:
        {
            meanval = Color4::zero();
            Color4unorm8 mn = Color4unorm8::one();
            Color4unorm8 mx = Color4unorm8::zero();
            bool anyFractionalAlpha = false;
            // Compute mean along rows to avoid overflow
            for (int y = 0; y < height; ++y) {
                const Color4unorm8* ptr = ((const Color4unorm8*)rawBytes) + (y * width);
                uint32 r = 0, g = 0, b = 0, a = 0;
                for (int x = 0; x < width; ++x) {
                    const Color4unorm8 i = ptr[x];
                    mn = mn.min(i);
                    mx = mx.max(i);
                    r += i.r.bits(); g += i.g.bits(); b += i.b.bits(); a += i.a.bits();
                    anyFractionalAlpha = anyFractionalAlpha || ((i.a.bits() < 255) && (i.a.bits() > 0));
                }
                meanval += Color4(float(r) * inv255Width, float(g) * inv255Width, float(b) * inv255Width, float(a) * inv255Width);
            }
            minval = mn;
            maxval = mx;
            meanval = meanval / (float)height;
            if (mn.a.bits() * encoding.readMultiplyFirst.a + encoding.readAddSecond.a * 255 == 255) {
                alphaFilter = AlphaFilter::ONE;
            } else if (anyFractionalAlpha || (encoding.readMultiplyFirst.a != 1.0f) || (encoding.readAddSecond.a != 0.0f)) {
                alphaFilter = AlphaFilter::BLEND;
            } else {
                alphaFilter = AlphaFilter::BINARY;
            }
        }
        break;

    case GL_RGBA32F:
        {
            meanval = Color4::zero();
            minval = Color4::one() * finf();
            maxval = Color4::one() * -finf();
            bool anyFractionalAlpha = false;
            // Compute mean along rows to avoid overflow
            for (int y = 0; y < height; ++y) {
                const Color4* ptr = ((const Color4*)rawBytes) + (y * width);
                Color4 M = Color4::zero();
                for (int x = 0; x < width; ++x) {
                    const Color4 c = ptr[x];
                    minval = minval.min(c);
                    maxval = maxval.max(c);
                    M += c;
                    anyFractionalAlpha = anyFractionalAlpha || (c.a > 0.0f && c.a < 1.0f);
                }
                meanval += M / (float)width;
            }
            meanval = meanval / (float)height;
            if (minval.a * encoding.readMultiplyFirst.a + encoding.readAddSecond.a == 1) {
                alphaFilter = AlphaFilter::ONE;
            } else if (anyFractionalAlpha || (encoding.readMultiplyFirst.a != 1.0f) || (encoding.readAddSecond.a != 0.0f)) {
                alphaFilter = AlphaFilter::BLEND;
            } else {
                alphaFilter = AlphaFilter::BINARY;
            }
        }
        break;

    case GL_SRGB8:
        {
            Color3unorm8 mn = Color3unorm8::one();
            Color3unorm8 mx = Color3unorm8::zero();
            meanval = Color4::zero();
            // Compute mean along rows to avoid overflow
            for (int y = 0; y < height; ++y) {
                const Color3unorm8* ptr = ((const Color3unorm8*)rawBytes) + (y * width);
                uint32 r = 0, g = 0, b = 0;
                for (int x = 0; x < width; ++x) {
                    Color3unorm8 i = ptr[x];
                    // SRGB_A->RGB_A
                    i.r = unorm8::fromBits(toRGB[i.r.bits()]);
                    i.g = unorm8::fromBits(toRGB[i.r.bits()]);
                    i.b = unorm8::fromBits(toRGB[i.r.bits()]);

                    mn = mn.min(i);
                    mx = mx.max(i);
                    r += i.r.bits(); g += i.g.bits(); b += i.b.bits();
                }
                meanval += Color4(float(r) * inv255Width, float(g) * inv255Width, float(b) * inv255Width, 1.0);
            }
            minval  = Color4(Color3(mn), 1.0f);
            maxval  = Color4(Color3(mx), 1.0f);
            meanval /= (float)height;
            meanval.a = 1.0f;
            alphaFilter = (1 * encoding.readMultiplyFirst.a + encoding.readAddSecond.a == 1) ? AlphaFilter::ONE : AlphaFilter::BLEND;
        }
        break;

    case GL_SRGB8_ALPHA8:
        {
            meanval = Color4::zero();
            Color4unorm8 mn = Color4unorm8::one();
            Color4unorm8 mx = Color4unorm8::zero();
            bool anyFractionalAlpha = false;
            // Compute mean along rows to avoid overflow
            for (int y = 0; y < height; ++y) {
                const Color4unorm8* ptr = ((const Color4unorm8*)rawBytes) + (y * width);
                uint32 r = 0, g = 0, b = 0, a = 0;
                for (int x = 0; x < width; ++x) {
                    Color4unorm8 i = ptr[x];
                    // SRGB_A->RGB_A
                    i.r = unorm8::fromBits(toRGB[i.r.bits()]);
                    i.g = unorm8::fromBits(toRGB[i.r.bits()]);
                    i.b = unorm8::fromBits(toRGB[i.r.bits()]);
                    mn = mn.min(i);
                    mx = mx.max(i);
                    r += i.r.bits(); g += i.g.bits(); b += i.b.bits(); a += i.a.bits();
                    anyFractionalAlpha = anyFractionalAlpha || ((i.a.bits() < 255) && (i.a.bits() > 0));
                }
                meanval += Color4(float(r) * inv255Width, float(g) * inv255Width, float(b) * inv255Width, float(a) * inv255Width);
            }
            minval = mn;
            maxval = mx;
            meanval = meanval / (float)height;
            if (anyFractionalAlpha) {
                alphaFilter = AlphaFilter::BLEND;
            } else if (mn.a.bits() == 255) {
                alphaFilter = AlphaFilter::ONE;
            } else {
                alphaFilter = AlphaFilter::BINARY;
            }
        }
        break;

    default:
        break;
    }

    ptb->unmap();

    debugAssertM(isNaN(minval.a) || (ptb->format()->alphaBits > 0) || ((minval.a == 1.0f) && (meanval.a == 1.0f) && (maxval.a == 1.0f)), "Cannot have a non-unit alpha for input without an alpha channel");
}

/** 
   @param bytesFormat OpenGL base format.

   @param bytesActualFormat OpenGL true format.  For compressed data,
   distinguishes the format that the data has due to compression.
 
   @param dataType Type of the incoming data from the CPU, e.g. GL_UNSIGNED_BYTES 
*/
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
            debugAssertM((target != GL_TEXTURE_RECTANGLE),
                "Compressed textures must be DIM_2D or DIM_2D.");

            glCompressedTexImage2DARB
                (target, mipLevel, bytesActualFormat, m_width, 
                 m_height, 0, (bytesPerPixel * ((m_width + 3) / 4) * ((m_height + 3) / 4)), 
                 rawBytes);

        } else {

            if (notNull(bytes)) {
                debugAssert(isValidPointer(bytes));
                debugAssertM(isValidPointer(bytes + (m_width * m_height - 1) * bytesPerPixel), 
                    "Byte array in Texture creation was too small");
            }

            // 2D texture, level of detail 0 (normal), internal
            // format, x size from image, y size from image, border 0
            // (normal), rgb color data, unsigned byte data, and
            // finally the data itself.
            glPixelStorei(GL_PACK_ALIGNMENT, 1);

            if (target == GL_TEXTURE_2D_MULTISAMPLE) {
                glTexImage2DMultisample(target, numSamples, ImageFormat, m_width, m_height, false);
            } else {
                debugAssertGLOk();
                glTexImage2D(target, mipLevel, ImageFormat, m_width, m_height, 0, bytesFormat, dataType, bytes);
                debugAssertGLOk();
            }
        }
        break;

    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
        debugAssert(isNull(bytes) || isValidPointer(bytes));
        glTexImage3D(target, mipLevel, ImageFormat, m_width, m_height, depth, 0, bytesFormat, dataType, bytes);
        break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        debugAssert(isNull(bytes) || isValidPointer(bytes));
        glTexImage3D(target, mipLevel, ImageFormat, m_width, m_height, 
            depth*6, 0, bytesFormat, dataType, bytes);
        break;
    default:
        debugAssertM(false, "Fell through switch");
    }

    if (freeBytes) {
        // Texture was resized; free the temporary.
        delete[] bytes;
    }
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
        } else if (alignmentOffset % 2 == 0) {
            newPackAlignment = 2;
        } else {
            newPackAlignment = 1;
        }
    }
    return newPackAlignment;
}

void Texture::toPixelTransferBuffer(shared_ptr<GLPixelTransferBuffer>& buffer, const ImageFormat* outFormat, int mipLevel, CubeFace face, bool runMapHooks) const {
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

shared_ptr<GLPixelTransferBuffer> Texture::toPixelTransferBuffer(const ImageFormat* outFormat, int mipLevel, CubeFace face) const {
    force();
    if (outFormat == ImageFormat::AUTO()) {
        outFormat = format();
    }
    debugAssertGLOk();
    alwaysAssertM( !isSRGBFormat(outFormat) || isSRGBFormat(format()), "glGetTexImage doesn't do sRGB conversion, so we need to first copy an RGB texture to sRGB on the GPU. However, this functionality is broken as of the time of writing this code");
    
    const bool cpuSRGBConversion = isSRGBFormat(format()) && !isSRGBFormat(outFormat) && (m_dimension == DIM_CUBE_MAP);

    if (isSRGBFormat(format()) && !isSRGBFormat(outFormat) && ! cpuSRGBConversion) {
        BEGIN_PROFILER_EVENT("G3D::Texture::toPixelTransferBuffer (slow path)");
        // Copy to non-srgb texture first, forcing OpenGL to perform the sRGB conversion in a pixel shader
        const shared_ptr<Texture>& temp = Texture::createEmpty("Temporary copy", m_width, m_height, outFormat, m_dimension, false, m_depth);
        Texture::copy(dynamic_pointer_cast<Texture>(const_cast<Texture*>(this)->shared_from_this()), temp);
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
        } else if (outFormat == ImageFormat::SRGBA8()) {
            outFormat = ImageFormat::RGBA8();
        }
    }
    int mipDepth = 1;
    if (dimension() == DIM_3D) {
        mipDepth = depth() >> mipLevel;
    } else if (dimension() == DIM_2D_ARRAY) {
        mipDepth = depth();
    }

    BEGIN_PROFILER_EVENT("GLPixelTransferBuffer::create");
    shared_ptr<GLPixelTransferBuffer> buffer = GLPixelTransferBuffer::create(width() >> mipLevel, height() >> mipLevel, outFormat, nullptr, mipDepth, GL_STATIC_READ);
    END_PROFILER_EVENT();
   
    toPixelTransferBuffer(buffer, outFormat, mipLevel, face);
    END_PROFILER_EVENT();
    return buffer;
}


shared_ptr<Image> Texture::toImage(const ImageFormat* outFormat, int mipLevel, CubeFace face) const {
    return Image::fromPixelTransferBuffer(toPixelTransferBuffer(outFormat, mipLevel, face));
}

/*
These methods were added for distributed rendering test by T.Beebe 4/30/19
*/

//void Texture::toPixelTransferBuffer2(shared_ptr<GLPixelTransferBuffer>& buffer, const ImageFormat* outFormat, int mipLevel, CubeFace face, bool runMapHooks) const {
//	debugAssertGLOk();
//	force();
//	if (outFormat == ImageFormat::AUTO()) {
//		outFormat = format();
//	}
//	debugAssertGLOk();
//	alwaysAssertM(!isSRGBFormat(outFormat) || isSRGBFormat(format()), "glGetTexImage doesn't do sRGB conversion, so we need to first copy an RGB texture to sRGB on the GPU. However, this functionality is broken as of the time of writing this code");
//
//	const bool cpuSRGBConversion = isSRGBFormat(format()) && !isSRGBFormat(outFormat) && (m_dimension == DIM_CUBE_MAP);
//
//	BEGIN_PROFILER_EVENT("G3D::Texture::toPixelTransferBuffer");
//
//	if (outFormat == format()) {
//		if (outFormat == ImageFormat::SRGB8()) {
//			outFormat = ImageFormat::RGB8();
//		}
//		else if (outFormat == ImageFormat::SRGBA8()) {
//			outFormat = ImageFormat::RGBA8();
//		}
//	}
//
//	// Need to call before binding in case an external
//	// application (CUDA) has this buffer mapped.
//	if (runMapHooks) {
//		buffer->runMapHooks();
//	}
//
//	glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer->glBufferID()); {
//		debugAssertGLOk();
//
//		glBindTexture(openGLTextureTarget(), openGLID()); {
//			debugAssertGLOk();
//			GLenum target;
//			if (isCubeMap()) {
//				target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face;
//			}
//			else {
//				// Not a cubemap
//				target = openGLTextureTarget();
//			}
//
//			bool alignmentNeedsToChange;
//			GLint oldPackAlignment;
//			const GLint newPackAlignment = getPackAlignment((int)buffer->stride(), oldPackAlignment, alignmentNeedsToChange);
//
//			debugAssertM(!((outFormat == ImageFormat::R32F()) && (m_encoding.format == ImageFormat::DEPTH32F())), "Read back DEPTH32F as DEPTH32F, not R32F");
//			if (alignmentNeedsToChange) {
//				glPixelStorei(GL_PACK_ALIGNMENT, newPackAlignment);
//				debugAssertGLOk();
//			}
//
//			BEGIN_PROFILER_EVENT("glGetTexImage");
//			debugAssertGLOk();
//			glGetTexImage(target, mipLevel, outFormat->openGLBaseFormat, outFormat->openGLDataFormat, 0);
//			debugAssertGLOk();
//			END_PROFILER_EVENT();
//			if (alignmentNeedsToChange) {
//				glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
//				debugAssertGLOk();
//			}
//
//		} glBindTexture(openGLTextureTarget(), GL_NONE);
//
//	} glBindBuffer(GL_PIXEL_PACK_BUFFER, GL_NONE);
//	debugAssertGLOk();
//
//	if (cpuSRGBConversion) {
//		BEGIN_PROFILER_EVENT("CPU sRGB -> RGB conversion");
//		// Fix sRGB
//		alwaysAssertM(outFormat == ImageFormat::RGB32F(), "CubeMap sRGB -> RGB conversion only supported for RGB32F format output");
//		Color3* ptr = (Color3*)buffer->mapReadWrite();
//		runConcurrently(0, int(buffer->size() / sizeof(Color3)), [&](int i) {
//			ptr[i] = ptr[i].sRGBToRGB();
//		});
//		buffer->unmap();
//		ptr = nullptr;
//		END_PROFILER_EVENT();
//	}
//
//	END_PROFILER_EVENT();
//}
//
//shared_ptr<GLPixelTransferBuffer> Texture::toPixelTransferBuffer2(const ImageFormat* outFormat, int mipLevel, CubeFace face) const {
//	force();
//	if (outFormat == ImageFormat::AUTO()) {
//		outFormat = format();
//	}
//	debugAssertGLOk();
//	alwaysAssertM(!isSRGBFormat(outFormat) || isSRGBFormat(format()), "glGetTexImage doesn't do sRGB conversion, so we need to first copy an RGB texture to sRGB on the GPU. However, this functionality is broken as of the time of writing this code");
//
//	const bool cpuSRGBConversion = isSRGBFormat(format()) && !isSRGBFormat(outFormat) && (m_dimension == DIM_CUBE_MAP);
//
//	if (isSRGBFormat(format()) && !isSRGBFormat(outFormat) && !cpuSRGBConversion) {
//		BEGIN_PROFILER_EVENT("G3D::Texture::toPixelTransferBuffer (slow path)");
//		// Copy to non-srgb texture first, forcing OpenGL to perform the sRGB conversion in a pixel shader
//		const shared_ptr<Texture>& temp = Texture::createEmpty("Temporary copy", m_width, m_height, outFormat, m_dimension, false, m_depth);
//		Texture::copy(dynamic_pointer_cast<Texture>(const_cast<Texture*>(this)->shared_from_this()), temp);
//		shared_ptr<GLPixelTransferBuffer> buffer = GLPixelTransferBuffer::create(m_width, m_height, outFormat);
//		temp->toPixelTransferBuffer(buffer, outFormat, mipLevel, face);
//
//		END_PROFILER_EVENT();
//		return buffer;
//	}
//
//	BEGIN_PROFILER_EVENT("G3D::Texture::toPixelTransferBuffer");
//	// OpenGL's sRGB readback is non-intuitive.  If we're reading from sRGB to sRGB, we actually read back using "RGB".
//	if (outFormat == format()) {
//		if (outFormat == ImageFormat::SRGB8()) {
//			outFormat = ImageFormat::RGB8();
//		}
//		else if (outFormat == ImageFormat::SRGBA8()) {
//			outFormat = ImageFormat::RGBA8();
//		}
//	}
//	int mipDepth = 1;
//	if (dimension() == DIM_3D) {
//		mipDepth = depth() >> mipLevel;
//	}
//	else if (dimension() == DIM_2D_ARRAY) {
//		mipDepth = depth();
//	}
//
//	BEGIN_PROFILER_EVENT("GLPixelTransferBuffer::create");
//	shared_ptr<GLPixelTransferBuffer> buffer = GLPixelTransferBuffer::create(width() >> mipLevel, height() >> mipLevel, outFormat, nullptr, mipDepth, GL_STATIC_READ);
//	END_PROFILER_EVENT();
//
//	toPixelTransferBuffer(buffer, outFormat, mipLevel, face);
//	END_PROFILER_EVENT();
//	return buffer;
//}
//
//shared_ptr<Image> Texture::toImage5(const ImageFormat* outFormat, int mipLevel, CubeFace face) const {
//	return Image::fromPixelTransferBuffer(toPixelTransferBuffer2(outFormat, mipLevel, face));
//}

/*
	End of methods added by T.Beebe 4/30/19
*/


void Texture::update(const shared_ptr<PixelTransferBuffer>& src, 
                     int mipLevel, CubeFace face, 
                     bool runMapHooks, 
                     size_t byteOffset, 
                     bool resizeTexture) {
    force();
    alwaysAssertM(format()->openGLBaseFormat == src->format()->openGLBaseFormat,
                    "Data must have the same number of channels as the texture: this = " + format()->name() + 
                    "  src = " + src->format()->name());

    // See if this PTB is already in GPU memory
    const shared_ptr<GLPixelTransferBuffer>& glsrc = dynamic_pointer_cast<GLPixelTransferBuffer>(src);

    if (resizeTexture) {
        resize(src->width(), src->height());
    }
    {
        glBindTexture(openGLTextureTarget(), openGLID());

        GLint previousPackAlignment;
        glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        const GLint xoffset = 0;
        const GLint yoffset = 0;
        const GLint zoffset = 0;
            
        GLenum target = openGLTextureTarget();
        if (isCubeMap()) {
            target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face;
        }

        const void* ptr = nullptr;

        if (notNull(glsrc)) {

            if (runMapHooks) {
                glsrc->runMapHooks();
            }
            // Bind directly instead of invoking bindRead(); see below for discussion
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glsrc->glBufferID());
            // The pointer is an offset in this case
            ptr = (void*)byteOffset;
        } else {
            // Regular path
            ptr = src->mapRead();
        }
        
        if (dimension() == DIM_2D || dimension() == DIM_CUBE_MAP) {
            debugAssertGLOk();
            glTexSubImage2D
                   (target,
                    mipLevel,
                    xoffset,
                    yoffset,
                    // Compute width/height off the texture to allow copying from a larger PBO.
                    width(),
                    height(),
                    src->format()->openGLBaseFormat,
                    src->format()->openGLDataFormat,
                    ptr);
            debugAssertGLOk();
        } else {
            alwaysAssertM(dimension() == DIM_3D || dimension() == DIM_2D_ARRAY, 
                "Texture::update only works with 2D, 3D, cubemap, and 2D array textures");
            debugAssertGLOk();
            glTexSubImage3D
                (target,
                    mipLevel,
                    xoffset,
                    yoffset,
                    zoffset,
                    src->width(),
                    src->height(),
                    src->depth(),
                    src->format()->openGLBaseFormat,
                    src->format()->openGLDataFormat,
                    ptr);
            debugAssertGLOk();
        }

        if (notNull(glsrc)) {
            // Creating the fence for this operation is VERY expensive because it causes a pipeline stall [on NVIDIA GPUs],
            // so we directly unbind the buffer instead of creating a fence.
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_NONE);
        } else {
            // We mapped the non-GL PTB, so unmap it 
            src->unmap();
        }

        glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
        glBindTexture(openGLTextureTarget(), 0);
    }
}


void Texture::setShaderArgs(UniformTable& args, const String& prefix, const Sampler& sampler) {
    force();
    const bool structStyle = ! prefix.empty() && (prefix[prefix.size() - 1] == '.');

    debugAssert(this != NULL);
    if (prefix.find('.') == std::string::npos) {
        args.setMacro(prefix + "notNull", true);
    } else if (structStyle) {
        args.setUniform(prefix + "notNull", true);
    }

    if (structStyle) {
        args.setUniform(prefix + "sampler", dynamic_pointer_cast<Texture>(shared_from_this()), sampler);
    } else {
        // Backwards compatibility
        args.setUniform(prefix + "buffer", dynamic_pointer_cast<Texture>(shared_from_this()), sampler);
    }

    args.setUniform(prefix + "readMultiplyFirst", m_encoding.readMultiplyFirst, true);
    args.setUniform(prefix + "readAddSecond", m_encoding.readAddSecond, true);

    if (structStyle && (m_dimension != DIM_2D_ARRAY) && (m_dimension != DIM_3D) && (m_dimension != DIM_CUBE_MAP_ARRAY)) {
        const Vector2 size((float)width(), (float)height());
        args.setUniform(prefix + "size", size);
        args.setUniform(prefix + "invSize", Vector2(1.0f, 1.0f) / size);
    } else {
        const Vector3 size((float)width(), (float)height(), (float)depth());
        args.setUniform(prefix + "size", size);
        args.setUniform(prefix + "invSize", Vector3(1.0f, 1.0f, 1.0f) / size);
    }
}

/////////////////////////////////////////////////////

Texture::Dimension Texture::toDimension(const String& s) {
    if (s == "DIM_2D") {
        return DIM_2D;
    } else if (s == "DIM_2D_ARRAY") {
        return DIM_2D_ARRAY;
    } else if (s == "DIM_2D_RECT") {
        return DIM_2D_RECT;
    } else if (s == "DIM_3D") {
        return DIM_3D;
    } else if (s == "DIM_CUBE_MAP") {
        return DIM_CUBE_MAP;
    } else if (s == "DIM_CUBE_MAP_ARRAY") {
        return DIM_CUBE_MAP_ARRAY;
    } else {
        debugAssertM(false, "Unrecognized dimension");
        return DIM_2D;
    }
}


const char* Texture::toString(Dimension d) {
    switch (d) {
    case DIM_2D:             return "DIM_2D";
    case DIM_2D_ARRAY:       return "DIM_2D_ARRAY";
    case DIM_3D:             return "DIM_3D";
    case DIM_2D_RECT:        return "DIM_2D_RECT";
    case DIM_CUBE_MAP:       return "DIM_CUBE_MAP";
    case DIM_CUBE_MAP_ARRAY: return "DIM_CUBE_MAP_ARRAY";
    default:
        return "ERROR";
    }
}

#ifdef G3D_ENABLE_CUDA

CUarray &Texture::cudaMap(unsigned int  usageflags){ //default should be: CU_GRAPHICS_REGISTER_FLAGS_NONE
    //CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST
    //TODO: Unregister ressource in destructor.
    
    if( m_cudaTextureResource != NULL && usageflags!=m_cudaUsageFlags){
        cuGraphicsUnregisterResource(m_cudaTextureResource);
    }
    
    if( m_cudaTextureResource == NULL || usageflags!=m_cudaUsageFlags){
        cuGraphicsGLRegisterImage(&m_cudaTextureResource, this->openGLID(), GL_TEXTURE_2D, usageflags );
        
        m_cudaUsageFlags = usageflags;
    }
    
    debugAssert(m_cudaIsMapped==false);
    
    cuGraphicsMapResources (1, &m_cudaTextureResource, 0);
    cuGraphicsSubResourceGetMappedArray ( &m_cudaTextureArray, m_cudaTextureResource, 0, 0);
    
    m_cudaIsMapped = true;
    
    return m_cudaTextureArray;
}


void Texture::cudaUnmap(){
    debugAssert(m_cudaIsMapped);

    cuGraphicsUnmapResources(1, &m_cudaTextureResource, 0);
    
    m_cudaIsMapped = false;
}

#endif

} // G3D
