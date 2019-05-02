#pragma once
#include <G3D-base/Image.h>
#include "G3D-base/PixelTransferBuffer.h"
#include "G3D-base/platform.h"
#include "../../external/freeimage.lib/include/FreeImagePlus.h"


namespace G3D {

	class ImageDist : public Image {

	protected:

		//fipImage*           m_image;
	
	public:

		ImageDist() : Image() {	}	

		//void set(const shared_ptr<PixelTransferBuffer>& buffer, int x, int y) {
		//	debugAssert(x >= 0 && x < width());
		//	debugAssert(y >= 0 && y < height());

		//	// Cannot copy between incompatible formats
		//	if (!m_format->canInterpretAs(buffer->format())) {
		//		return;
		//	}

		//	BYTE* pixels(m_image->accessPixels());
		//	debugAssert(pixels);

		//	if (notNull(pixels)) {
		//		// The area we want to set and clip to image bounds
		//		const Rect2D& rect = Rect2D::xywh((float)x, (float)y, (float)buffer->width(), (float)buffer->height()).intersect(bounds());

		//		if (!rect.isEmpty()) {
		//			const size_t bytesPerPixel = iCeil(buffer->format()->cpuBitsPerPixel / 8.0f);
		//			const size_t rowStride = size_t(rect.width() * bytesPerPixel);
		//			const size_t columnOffset = size_t(rect.x0()    * bytesPerPixel);

		//			const uint8* src = static_cast<const uint8*>(buffer->mapRead());
		//			debugAssert(notNull(src));

		//			// For each row in the rectangle
		//			for (int row = 0; row < rect.height(); ++row) {
		//				BYTE* dst = m_image->getScanLine(int(rect.y1()) - row - 1) + columnOffset;
		//				System::memcpy(dst, src + (buffer->width() * (row + (int(rect.y0()) - y)) + (int(rect.x0()) - x)) * bytesPerPixel, rowStride);
		//			}
		//			buffer->unmap();
		//		}
		//	}
		//}

		//static shared_ptr<PixelTransferBuffer> toPixelTransferBuffer(Rect2D rect, shared_ptr<PixelTransferBuffer> buffer) const {
		//	// clip to bounds of image
		//	rect = rect.intersect(bounds());

		//	if (rect.isEmpty()) {
		//		return nullptr;
		//	}

		//	if (isNull(buffer)) {
		//		buffer = CPUPixelTransferBuffer::create((int)rect.width(), (int)rect.height(), m_format, AlignedMemoryManager::create(), 1, 1);
		//	}
		//	else {
		//		debugAssert(buffer->width() == rect.width());
		//		debugAssert(buffer->height() == rect.height());
		//		debugAssert(buffer->format() == m_format);
		//	}

		//	BYTE* pixels = m_image->accessPixels();
		//	if (pixels) {
		//		const size_t bytesPerPixel = iCeil(buffer->format()->cpuBitsPerPixel / 8.0f);
		//		const size_t rowStride = int(rect.width())  * bytesPerPixel;
		//		const size_t offsetStride = int(rect.x0())     * bytesPerPixel;

		//		debugAssert(isFinite(rect.width()) && isFinite(rect.height()));

		//		uint8* ptr = (uint8*)buffer->mapWrite();
		//		for (int row = 0; row < int(rect.height()); ++row) {
		//			// Note that we flip vertically while copying
		//			System::memcpy(ptr + buffer->rowOffset(row), m_image->getScanLine(int(rect.height()) - 1 - (row + int(rect.y0()))) + offsetStride, rowStride);
		//		}
		//		buffer->unmap();
		//	}

		//	return buffer;
		//}

		static shared_ptr<ImageDist> fromPixelTransferBuffer(const shared_ptr<PixelTransferBuffer>& buffer, int x, int y) {
			const shared_ptr<ImageDist>& img = create(buffer->width(), buffer->height(), buffer->format());
			img->set(buffer,x,y);
			return img;
		}

		static shared_ptr<ImageDist> create(int width, int height, const ImageFormat* imageFormat) {
			alwaysAssertM(notNull(imageFormat), "imageFormat may not be ImageFormat::AUTO() or NULL");
			const shared_ptr<ImageDist>& img = createShared<ImageDist>();
			img->setSize(width, height, imageFormat);
			return img;
		}
	};
}