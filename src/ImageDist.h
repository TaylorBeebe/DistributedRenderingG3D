#pragma once
#include <G3D-base/Image.h>
#include "G3D-base/PixelTransferBuffer.h"

namespace G3D {

	class ImageDist : public Image {

	public:

		ImageDist() : Image() { }	

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