#pragma once
#include <G3D/G3D.h>
#include <map>
#include <vector>
#include <list>

namespace RemoteRenderer{

    G3D::NetAddress SERVER;
    G3D::NetAddress CLIENT;
    G3D::NetAddress N1;
    G3D::NetAddress N2;
    G3D::NetAddress N3;

    // Supported network packet types
    enum PacketType {
        TRANSFORM = 0x0,
        FRAME = 0x1
    }

    // a transform is a 7 tuple
    struct {
        uint id;
        float x;
        float y;
        float z;
        float yaw;
        float pitch;
        float roll;
    } transform_t;

}