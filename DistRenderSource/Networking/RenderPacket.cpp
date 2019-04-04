#include "RenderPacket.h"

using namespace RemoteRenderer;

namespace RemoteRenderer{

    // empty packet
    RenderPacket::RenderPacket(PacketType t, uint bid) : type(t), batch_id(bid) {}

    // from binary input
    RenderPacket::RenderPacket(PacketType t, G3D::BinaryInput& header, G3D::BinaryInput& body) : type(t) {
        setHeader(header);
        setBody(body);
    }

    void RenderPacket::setHeader(G3D::BinaryInput& b){
        b.beginBits();

        batch_id = b.readUInt32();

        // switch(type){
        //     case PacketType::FRAME:
        //         // maybe height and width
        //         break;
        //     case PacketType::FRAGMENT:
                
        //         break;
        //     default:
        //         // no need to read anything else
        //         break;
        // }

        b.endBits();
    }

    void RenderPacket::setHeader(G3D::BinaryOutput& b){
        setHeader(new BinaryInput((char*) b.getCArray(), b.length(), G3D::G3DEndian::G3D_BIG_ENDIAN, constants::COMPRESS_NETWORK_DATA, true));
    }

    void RenderPacket::setHeader(char* dat, long len){
        setHeader(new BinaryInput(dat, len, G3D::G3DEndian::G3D_BIG_ENDIAN, constants::COMPRESS_NETWORK_DATA, true));
    }

    void RenderPacket::setBody(G3D::BinaryInput& b){
        data = (char*) b.getCArray();
        data_len = b.length();
    }

    void RenderPacket::setBody(G3D::BinaryOutput& b){
        data = (char*) b.getCArray();
        data_len = b.length();
    }

    void RenderPacket::setBody(char* dat, long len){
        data = dat;
        data_len = len;
    }

    G3D::BinaryOutput& RenderPacket::serializeHeader(){
        G3D::BinaryOutput* header ();
        header.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

        header.beginBits();

        // write body
        header.writeBits((uint*) data, data_len);

        header.endBits();

        if(constants::COMPRESS_NETWORK_DATA) header.compress();

        return header;
    }

    G3D::BinaryOutput& RenderPacket::serializeBody(){
        G3D::BinaryOutput body ();
        body.setEndian(G3D::G3DEndian::G3D_BIG_ENDIAN);

        body.beginBits();

        // write body
        body.writeBits((uint*) data, data_len);

        body.endBits();

        if(constants::COMPRESS_NETWORK_DATA) body.compress();

        return body;
    }

}