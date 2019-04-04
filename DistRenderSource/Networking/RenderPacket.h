#pragma once
#include "RemoteRenderer.h"

namespace RemoteRenderer{

    // Wrapper class for binary data for easy reads and writes
    class RenderPacket {
        protected:
            PacketType type;
            uint batch_id;

            char* data;
            long data_len;
            
        public:
            // create an empty packcet
            RenderPacket(PacketType t, uint bid);

            // from raw binary data
            RenderPacket(PacketType t, G3D::BinaryInput& header, G3D::BinaryInput& body);

            uint getBatchId() { return batch_id; }
            PacketType getPacketType() { return type; }

            // get the inner body of the message
            G3D::BinaryInput& getBody() {
                return G3D::BinaryInput(data, data_len, G3D::G3DEndian::G3D_BIG_ENDIAN, constants::COMPRESS_NETWORK_DATA, true);
            }

            void setHeader(G3D::BinaryOutput& dat);
            void setHeader(G3D::BinaryInput& dat);
            void setHeader(char* dat, long len);

            void setBody(G3D::BinaryOutput& dat);
            void setBody(G3D::BinaryInput& dat);
            void setBody(char* dat, long len);
            
            G3D::BinaryOutput& serializeHeader();
            G3D::BinaryOutput& serializeBody();       
    }

}