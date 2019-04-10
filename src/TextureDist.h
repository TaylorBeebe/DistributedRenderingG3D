#pragma once
#include <G3D/G3D.h>

class ImageDist : public Texture {

public:

	shared_ptr<Image> toImage5(const ImageFormat* outFormat, int w, int h, int mipLevel, CubeFace face) const;
	//void toPixelTransferBuffer(shared_ptr<GLPixelTransferBuffer>& buffer, const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X) const;
};