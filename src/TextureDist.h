#pragma once
#include <G3D/G3D.h>

class TextureDist : public Texture {

public:
	shared_ptr<GLPixelTransferBuffer> toPixelTransferBuffer(const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X);

	// Overload writes output to passed in pixelTransferBuffer
	void toPixelTransferBuffer(shared_ptr<GLPixelTransferBuffer>& buffer, const ImageFormat* outFormat = ImageFormat::AUTO(), int mipLevel = 0, CubeFace face = CubeFace::POS_X);
	
	shared_ptr<Image> toImage5(const ImageFormat* outFormat, int w, int h, int mipLevel, CubeFace face) const;
	
};