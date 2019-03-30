#include "RenderPacket.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{

    //////////////////////////////////////////////
    //              RENDER PACKETS
    //////////////////////////////////////////////

    RenderPacket::RenderPacket(PacketType t, uint bid) : type(t), batch_id(bid) {}

    RenderPacket::RenderPacket(char* data, size_t len, bool c) : compressed(c){

        bitdata = new G3D::BinaryInput(data, len, G3D::G3DEndian::G3D_BIG_ENDIAN, compressed, true);

        // read the header
        bitdata->beginBits();
        batch_id = bitdata->readUInt32();
        type = bitdata->readUInt32();
        bitdata->endBits();
    }

    G3D::BinaryOutput* RenderPacket::toBinary(){

        G3D::BinaryOutput newstream = new G3D::BinaryOutput();
        newstream.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

        if(bitdata != NULL){
            newstream.beginBits();

            long length = bitdata->getLength();
            // sketchy, still wondering about input datatype
            newstream.writeBits(bitdata->readBits(length), length);

            newstream.endBits();
            if(compressed) newstream.compress();
        }

        return &newstream;
    }

    //////////////////////////////////////////////
    //             TRANSFORM PACKETS
    //////////////////////////////////////////////

    TransformPacket::TransformPacket(uint batch_id) : RenderPacket(TRANSFORM, batch_id) {}

    TransformPacket::TransformPacket(RenderPacket* rpacket) {
        if(rpacket->getPacketType() == TRANSFORM) TransformPacket(rpacket->getBatchId(), rpacket->getBitStream());
        else TransformPacket(rpacket->getBatchId()); // return empty packet, probably should print something here
    }

    TransformPacket::TransformPacket(uint batch_id, G3D::BinaryInput* bitstream) : RenderPacket(TRANSFORM, batch_id), bitdata(bitstream) {

        bitdata = bitstream;

        bitdata->reset();
        bitdata->beginBits();

        // skip batch and type 
        bitdata->readUInt32();
        bitdata->readUInt32();

        while(bitdata->hasMore()){
            uint id = bitdata->readUInt32();
            float x = bitdata->readFloat32();
            float y = bitdata->readFloat32();
            float z = bitdata->readFloat32();
            float yaw = bitdata->readFloat32();
            float pitch = bitdata->readFloat32();
            float roll = bitdata->readFloat32();

            addTransform(id,x,y,z,yaw,pitch,roll);
        }

        bitdata->endBits();
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
        if(compressed) bitstream.compress();

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

    FramePacket::FramePacket(RenderPacket* rpacket) {
        if(rpacket->getPacketType() == FRAME) FramePacket(rpacket->getBatchId(), rpacket->getBitStream());
        else FramePacket(rpacket->getBatchId()); // return empty packet, probably should print something here
    }

    FramePacket::FramePacket(uint batch_id, G3D::BinaryInput* bitstream) : RenderPacket(FRAME, batch_id), bitdata(bitstream){
        // TODO: Impl

        bitdata->reset();
        bitdata->beginBits();

        bitdata->endBits();
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