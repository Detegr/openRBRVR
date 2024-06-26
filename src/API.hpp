#pragma once

#include <cstdint>

enum ApiOperation : uint64_t {
    API_VERSION = 0x0,
    RECENTER_VR_VIEW = 0x1,
    TOGGLE_DEBUG_INFO = 0x2,
    OPENXR_REQUEST_INSTANCE_EXTENSIONS = 0x4,
    OPENXR_REQUEST_DEVICE_EXTENSIONS = 0x8,
    GET_VR_RUNTIME = 0x10,
    MOVE_SEAT = 0x20,
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
