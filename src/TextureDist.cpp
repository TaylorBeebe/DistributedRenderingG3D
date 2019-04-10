#include "TextureDist.h"

shared_ptr<Image> toImage5(const ImageFormat* outFormat, int w, int h, int mipLevel, CubeFace face) {
	//not finding parent function
	return Image::fromPixelTransferBuffer(toPixelTransferBuffer(outFormat, mipLevel, face));
}