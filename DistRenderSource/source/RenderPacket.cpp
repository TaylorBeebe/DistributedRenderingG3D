#include "RenderPacket.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{


	//////////////////////////////////////////////
	//			   TRANSFORM PACKETS
	//////////////////////////////////////////////

	TransformPacket::TransformPacket(uint batch_id) : RenderPacket(TRANSFORM, batch_id) {}

	TransformPacket::TransformPacket(uint batch_id, G3D::BinaryInput* bitstream) : RenderPacket(TRANSFORM, batch_id) {

		bitstream->beginBits();

		// skip batch and type 
		bitstream->readUInt32();
		bitstream->readUInt32();

		while(bitstream->hasMore()){
			uint id = bitstream->readUInt32();
			float x = bitstream->readFloat32();
			float y = bitstream->readFloat32();
			float z = bitstream->readFloat32();
			float yaw = bitstream->readFloat32();
			float pitch = bitstream->readFloat32();
			float roll = bitstream->readFloat32();

			addTransform(id,x,y,z,yaw,pitch,roll);
		}

		bitstream->endBits();
	}

	G3D::BinaryOutput* TransformPacket::toBinary(){

		G3D::BinaryOutput bitstream = new G3D::BinaryOutput();
		bitstream.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

		bitstream.beginBits();

		// write type of data
		bitstream.writeUInt32(batch_id);
		bitstream.writeUInt32(TRANSFORM);

		// write each transform as an id followed by 6 floats
		for (std::list<transform_t*>::iterator it=transforms.begin(); it != transforms.end(); ++it){

			transform_t* transform = *it;

			bitstream.writeUInt32(transform->id);
			bitstream.writeFloat32(transform->x);
			bitstream.writeFloat32(transform->y);
			bitstream.writeFloat32(transform->z);
			bitstream.writeFloat32(transform->yaw);
			bitstream.writeFloat32(transform->pitch);
			bitstream.writeFloat32(transform->roll);

		}

		bitstream.endBits();
		// compress bits ? 

		return bitstream;
	}

	void TransformPacket::addTransform(uint id, G3D::CFrame* frame){
		float x,y,z,yaw,pitch,roll;
		frame->getXYZYPRRadians(x,y,z,yaw,pitch,roll)

		transform_t* transform = new transform_t();

		transform->id = id;
		transform->x = x;
		transform->y = y;
		transform->z = z;
		transform->yaw = yaw;
		transform->pitch = pitch;
		transform->roll = roll;

		addTransform(t);
	}

	void TransformPacket::addTransform(transform_t* t){
		transforms.push_back(transform);
	}


	//////////////////////////////////////////////
	//			   	 FRAME PACKETS
	//////////////////////////////////////////////

	FramePacket::FramePacket(uint batch_id, uint w, uint h) : RenderPacket(FRAME, batch_id), width(w), height(h) {}

	FramePacket::FramePacket(uint batch_id, G3D::BinaryInput* bitstream) : RenderPacket(FRAME, batch_id){
		// TODO: Impl
	}

	G3D::BinaryOutput* FramePacket::toBinary(){
		// TODO: Impl

		G3D::BinaryOutput stream = new G3D::BinaryOutput();
		stream.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

		stream.beginBits();

		// write type of data
		stream.writeUInt32(batch_id);
		stream.writeUInt32(FRAME);

		// write height and width
		stream.writeUInt32(width);
		stream.writeUInt32(height);

		stream.endBits();
	}


}