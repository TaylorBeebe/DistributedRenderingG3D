#pragma once
#include "RemoteRenderer.h"

namespace RemoteRenderer{

    // ABSTRACT RENDER PACKET CLASS
    // an abstraction of a packet which has a type and a batch id
    // each subclass should override toBinary() so that it can be sent on the network
    class RenderPacket {
        protected:
            PacketType type;
            uint batch_id;

            G3D::BinaryInput* bitdata;
            bool compressed = false;
            
        public:
            // empty packet
            RenderPacket(PacketType t, uint bid);
            // from raw binary data
            RenderPacket(char* data, size_t len, bool c);

            virtual G3D::BinaryOutput* toBinary();

            G3D::BinaryInput* getBitStream() { return bitdata; }
            uint getBatchId() { return batch_id; }
            PacketType getPacketType() { return type; }
            bool isCompressed() { return compressed; }
    }

    // TRANSFORM PACKET CLASS
    // holds a list of transforms 
    // each transform 
    class TransformPacket : public RenderPacket {
        private:
            std::list<transform_t*> transforms;
        public:
            TransformPacket(uint bid);
            TransformPacket(RenderPacket* rpacket);
            TransformPacket(uint bid, G3D::BinaryInput* bitstream);

            G3D::BinaryOutput* toBinary() override;

            void addTransform(uint id, G3D::CFrame* frame);

            std::list<transform_t*> getTransforms() { return transforms; }
    }

    // FRAME PACKET CLASS
    // holds a frame or frame fragment with a specified height and width
    class FramePacket : public RenderPacket {
        private:
            uint width;
            uint height;

            // replace with image data
            int* frame; 

        public:
            FramePacket(uint bid);
            FramePacket(uint bid, uint w, uint h);
            FramePacket(RenderPacket* rpacket);
            FramePacket(uint bid, G3D::BinaryInput* bitstream);

            G3D::BinaryOutput* toBinary() override;
    }
}