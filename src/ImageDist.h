#pragma once
#include <G3D-base/Image.h>
#include "G3D-base/PixelTransferBuffer.h"
#include "G3D-base/platform.h"
#include "../../external/freeimage.lib/include/FreeImagePlus.h"


namespace DistributedRenderer {

	class ImageDist : public Image {
	
	public:

		ImageDist() : Image() {	}	

		fipImage* image() {
			return m_image;
		}

		static const ImageFormat* determineImageFormat(const fipImage* image) {
			debugAssert(image->isValid() && image->getImageType() != FIT_UNKNOWN);

			const ImageFormat* imageFormat = nullptr;
			const FREE_IMAGE_TYPE t = image->getImageType();
			switch (t) {
			case FIT_BITMAP:
			{
				const int bits = image->getBitsPerPixel();
				switch (bits)
				{
				case 1: // Fall through
				case 4: // Fall through
				case 8:
				{
					const FREE_IMAGE_COLOR_TYPE c = image->getColorType();
					if ((c == FIC_PALETTE) || (c == FIC_RGB)) {
						if (image->isTransparent()) {
							imageFormat = ImageFormat::RGBA8();
						}
						else {
							imageFormat = ImageFormat::RGB8();
						}
					}
					else if (c == FIC_RGBALPHA) {
						imageFormat = ImageFormat::RGBA8();
					}
					else {
						imageFormat = ImageFormat::L8();
					}
				}
				break;


				case 16:
					// todo: find matching image format
					debugAssertM(false, "Unsupported bit depth loaded.");
					break;

				case 24:
					imageFormat = ImageFormat::RGB8();
					break;

				case 32:
					imageFormat = ImageFormat::RGBA8();
					break;

				default:
					debugAssertM(false, "Unsupported bit depth loaded.");
					break;
				}
				break;
			}

			case FIT_UINT16:
				imageFormat = ImageFormat::L16();
				break;

			case FIT_FLOAT:
				imageFormat = ImageFormat::L32F();
				break;

			case FIT_RGBF:
				imageFormat = ImageFormat::RGB32F();
				break;

			case FIT_RGBAF:
				imageFormat = ImageFormat::RGBA32F();
				break;

			case FIT_INT16:
			case FIT_UINT32:
			case FIT_INT32:
			case FIT_DOUBLE:
			case FIT_RGB16:
			case FIT_RGBA16:
			case FIT_COMPLEX:
			default:
				debugAssertM(false, "Unsupported FreeImage type loaded.");
				break;
			}

			// Cannot invoke the color space accessor here if only reading metadata,
			// so cast to DIB instead.
			if (FreeImage_GetICCProfile(*(fipImage*)image)->flags & FIICC_COLOR_IS_CMYK) {
				debugAssertM(false, "Unsupported FreeImage color space (CMYK) loaded.");
				imageFormat = nullptr;
			}

			return imageFormat;
		}

		shared_ptr<PixelTransferBuffer> toPixelTransferBuffer(Rect2D rect, shared_ptr<PixelTransferBuffer> buffer = nullptr) {
			// clip to bounds of image
			rect = rect.intersect(bounds());

			if (rect.isEmpty()) {
				return nullptr;
			}

			if (isNull(buffer)) {
				buffer = CPUPixelTransferBuffer::create((int)rect.width(), (int)rect.height(), m_format, AlignedMemoryManager::create(), 1, 1);
			}
			else {
				debugAssert(buffer->width() == rect.width());
				debugAssert(buffer->height() == rect.height());
				debugAssert(buffer->format() == m_format);
			}

			BYTE* pixels = m_image->accessPixels();
			if (pixels) {
				const size_t bytesPerPixel = iCeil(buffer->format()->cpuBitsPerPixel / 8.0f);
				const size_t rowStride = int(rect.width())  * bytesPerPixel;
				const size_t offsetStride = int(rect.x0())     * bytesPerPixel;

				debugAssert(isFinite(rect.width()) && isFinite(rect.height()));

				uint8* ptr = (uint8*)buffer->mapWrite();
				for (int row = 0; row < int(rect.height()); ++row) {
					// Note that we flip vertically while copying
					System::memcpy(ptr + buffer->rowOffset(row), m_image->getScanLine(rect.y0() + rect.height() - 1 - row + offsetStride), rowStride);

					//int(rect.height()) - 1 - (row + int(rect.y0()))) + offsetStride, rowStride);
				}
				buffer->unmap();
			}

			return buffer;
		}

		static shared_ptr<ImageDist> fromBinaryInput(BinaryInput& bi, const ImageFormat* imageFormat) {
			const shared_ptr<ImageDist>& img = createShared<ImageDist>();

			fipMemoryIO memoryIO(const_cast<uint8*>(bi.getCArray() + bi.getPosition()), static_cast<DWORD>(bi.getLength() - bi.getPosition()));

			if (!img->m_image->loadFromMemory(memoryIO)) {

				throw Image::Error("Unsupported file format or unable to allocate FreeImage buffer", bi.getFilename());
				return nullptr;
			}

			const ImageFormat* detectedFormat = determineImageFormat(img->m_image);

			if (isNull(detectedFormat)) {
				throw Image::Error("Loaded image pixel format does not map to any existing ImageFormat", bi.getFilename());
				return nullptr;
			}

			if (imageFormat == ImageFormat::AUTO()) {
				img->m_format = detectedFormat;
			}
			else {
				debugAssert(detectedFormat->canInterpretAs(imageFormat));
				if (!detectedFormat->canInterpretAs(imageFormat)) {
					throw Image::Error(G3D::format("Loaded image pixel format (%s) is not compatible with requested ImageFormat (%s)",
						detectedFormat->name().c_str(), imageFormat->name().c_str()), bi.getFilename());
					return nullptr;
				}
				img->m_format = imageFormat;
			}

			// Convert 1-bit images to 8-bit so that they correspond to an OpenGL format
			if ((img->m_image->getImageType() == FIT_BITMAP) && (img->m_image->getBitsPerPixel() < 8)) {
				const bool success = img->convertToL8();
				(void)success;
				debugAssert(success);
				debugAssert(img->m_image->getBitsPerPixel() == 8);
			}

			// Convert palettized images so row data can be copied easier
			if (img->m_image->getColorType() == FIC_PALETTE) {
				switch (img->m_image->getBitsPerPixel()) {
				case 1:
					img->convertToL8();
					break;

				case 8:
				case 24:
				case 32:
					if (img->m_image->isTransparent()) {
						img->convertToRGBA8();
					}
					else {
						img->convertToRGB8();
					}
					break;

				default:
					throw Image::Error("Loaded image data in unsupported palette format", bi.getFilename());
					return shared_ptr<ImageDist>();
				}
			}

			return img;
		}

		static shared_ptr<ImageDist> fromPixelTransferBuffer(const shared_ptr<PixelTransferBuffer>& buffer) {
			const shared_ptr<ImageDist>& img = create(buffer->width(), buffer->height(), buffer->format());
			img->set(buffer);
			return img;
		}

		static shared_ptr<ImageDist> create(int width, int height, const ImageFormat* imageFormat) {
			alwaysAssertM(notNull(imageFormat), "imageFormat may not be ImageFormat::AUTO() or NULL");
			const shared_ptr<ImageDist>& img = createShared<ImageDist>();
			img->setSize(width, height, imageFormat);
			return img;
		}

		static shared_ptr<PixelTransferBuffer> CombineImages(const Array<shared_ptr<ImageDist> >& images) {
			if (images.size() == 0) {
				return nullptr;
			}

			const int width = images[0]->width();
			const int perImageHeight = images[0]->height();
			const int height = images[0]->height() * images.size();

			const shared_ptr<CPUPixelTransferBuffer>& buffer = CPUPixelTransferBuffer::create(width, height, images[0]->format(), AlignedMemoryManager::create(), images.size(), 1);

			const int bytesPerPixel = iCeil(buffer->format()->cpuBitsPerPixel / 8.0f);
			const int memoryPerImage = width * perImageHeight * bytesPerPixel;
			const int memoryPerRow = width * bytesPerPixel;

			uint8 *data = static_cast<uint8*>(buffer->buffer());
			for (int i = 0; i < images.size(); ++i) {
				fipImage *currentImage = images[i]->m_image;
				for (int row = 0; row < perImageHeight; ++row) {
					System::memcpy(data + (i * memoryPerImage) + row * memoryPerRow, currentImage->getScanLine(perImageHeight - 1 - row), memoryPerRow);
				}
			}

			return buffer;
		}
	};


}