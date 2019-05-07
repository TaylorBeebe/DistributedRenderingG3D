#pragma once
#include <G3D-base/Image.h>
#include "G3D-base/PixelTransferBuffer.h"
#include "G3D-base/platform.h"
#include "../../external/freeimage.lib/include/FreeImagePlus.h"


namespace G3D {

	class ImageDist : public Image {
	
	public:

		ImageDist() : Image() {	}	

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