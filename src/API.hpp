#pragma once

#include <cstdint>

enum ApiOperation : uint64_t {
    API_VERSION = 0x0,
    RECENTER_VR_VIEW = 0x1,
    TOGGLE_DEBUG_INFO = 0x2,
    NOTIFY_VERTEX_SHADER = 0x4,
    RESERVED2 = 0x8,
    GET_VR_RUNTIME = 0x10,
    MOVE_SEAT = 0x20,
    SET_REVERSE_Z = 0x40,
    GET_VIEW_INFO = 0x80,
};

enum SeatMovement : uint64_t {
    MOVE_SEAT_STOP = 0,
    MOVE_SEAT_FORWARD = 1,
    MOVE_SEAT_RIGHT = 2,
    MOVE_SEAT_BACKWARD = 3,
    MOVE_SEAT_LEFT = 4,
    MOVE_SEAT_UP = 5,
    MOVE_SEAT_DOWN = 6,
};

struct ViewInfo {
    uint32_t struct_size;
    uint32_t requested_target;
    uint32_t width;
    uint32_t height;

    // Each is memcpy'd contents of a D3DMATRIX
    float projection[16];
    float pose[16];
    float eye_pos[16];
    float horizon_lock[16];
};
