#pragma once
#include "RemoteRenderer.h"
#include <list>

namespace RemoteRenderer{

	enum PacketType {
		TRANSFORM = 0x0,
		FRAME = 0x1
	}

	struct {
		uint id;
		float x;
		float y;
		float z;
		float yaw;
		float pitch;
		float roll;
	} transform_t;

	// ABSTRACT RENDER PACKET CLASS
	// an abstraction of a packet which has a type and a batch id
	// each subclass should override toBinary() so that it can be sent on the network
	class RenderPacket {
		protected:
			PacketType type;
			uint batch_id;
		public:
			RenderPacket(PacketType t, uint bid): type(t), batch_id(bid) {}

			G3D::BinaryOutput* toBinary() {
				return new G3D::BinaryOutput();
			}

			uint getBatchId() { return batch_id; }
			PacketType getPacketType() { return type; }
	}

	// TRANSFORM PACKET CLASS
	// holds a list of transforms 
	// each transform 
	class TransformPacket : public RenderPacket{
		public:
			std::list<transform_t*> transforms;

			TransformPacket(uint bid);
			TransformPacket(uint bid, G3D::BinaryInput* bitstream);

			G3D::BinaryOutput* toBinary() override;

			void addTransform(uint id, G3D::CFrame* frame);

	}

	// FRAME PACKET CLASS
	// holds a frame or frame fragment with a specified height and width
	class FramePacket : public RenderPacket{
		private:
			uint width;
			uint height;

			// replace with image data
			int* frame; 

		public:
			FramePacket(uint bid, uint w, uint h);
			FramePacket(uint bid, G3D::BinaryInput* bitstream);

			G3D::BinaryOutput* toBinary() override;
	}
}