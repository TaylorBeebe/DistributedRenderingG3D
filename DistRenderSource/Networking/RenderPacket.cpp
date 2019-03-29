#include "RenderPacket.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{

    //////////////////////////////////////////////
    //                RAW PACKETS
    //////////////////////////////////////////////

    G3D::BinaryOutput* RawPacket::toBinary(){

        G3D::BinaryOutput newstream = new G3D::BinaryOutput();
        newstream.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

        newstream.beginBits();

        long length = bitstream->getLength();
        // sketchy, still wondering about input datatype
        newstream.writeBits(bitstream->readBits(length), length);

        newstream.endBits();

        if(compressed) newstream.compress();

        return &newstream;
    }


    //////////////////////////////////////////////
    //             TRANSFORM PACKETS
    //////////////////////////////////////////////

    TransformPacket::TransformPacket(uint batch_id) : RenderPacket(TRANSFORM, batch_id) {}

    TransformPacket::TransformPacket(RawPacket* raw_packet) {
        if(raw_packet->getPacketType() == TRANSFORM) TransformPacket(raw_packet->getBatchId(), raw_packet->getBitStream());
        else TransformPacket(raw_packet->getBatchId()); // return empty packet, probably should print something here
    }

    TransformPacket::TransformPacket(uint batch_id, G3D::BinaryInput* bitstream) : RenderPacket(TRANSFORM, batch_id) {

        bitstream->reset();
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

        return &bitstream;
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
    //               FRAME PACKETS
    //////////////////////////////////////////////

    FramePacket::FramePacket(uint batch_id) : FramePacket(batch_id, SCREEN_WIDTH, SCREEN_HEIGHT) {}

    FramePacket::FramePacket(uint batch_id, uint w, uint h) : RenderPacket(FRAME, batch_id), width(w), height(h) {}

    FramePacket::FramePacket(RawPacket* raw_packet) {
        if(raw_packet->getPacketType() == FRAME) FramePacket(raw_packet->getBatchId(), raw_packet->getBitStream());
        else FramePacket(raw_packet->getBatchId()); // return empty packet, probably should print something here
    }

    FramePacket::FramePacket(uint batch_id, G3D::BinaryInput* bitstream) : RenderPacket(FRAME, batch_id){
        // TODO: Impl
        bitstream->reset();
        bitstream->beginBits();

        bitstream->endBits();
    }

    G3D::BinaryOutput* FramePacket::toBinary(){
        // TODO: Impl

        G3D::BinaryOutput bitstream = new G3D::BinaryOutput();
        bitstream.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

        bitstream.beginBits();

        // write type of data
        bitstream.writeUInt32(batch_id);
        bitstream.writeUInt32(FRAME);

        // write height and width
        bitstream.writeUInt32(width);
        bitstream.writeUInt32(height);

        bitstream.endBits();

        return &bitstream;
    }


}