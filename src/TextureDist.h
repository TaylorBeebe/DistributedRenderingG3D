#pragma once
#include <G3D/G3D.h>

class TextureDist : public Texture {

public:

	void toPixelTransferBuffer(shared_ptr<GLPixelTransferBuffer>& buffer, const ImageFormat* outFormat, int mipLevel, CubeFace face) {
		Texture::toPixelTransferBuffer(buffer, outFormat, mipLevel, face);
	}

	shared_ptr<GLPixelTransferBuffer> toPixelTransferBuffer(const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X) {
		return Texture::toPixelTransferBuffer(outFormat, mipLevel, face);
	}

	shared_ptr<Image> toImage5(const ImageFormat* outFormat, int w, int h, int mipLevel, CubeFace face) const {
		return Image::fromPixelTransferBuffer(Texture::toPixelTransferBuffer(outFormat, mipLevel, face));
	}
	
};