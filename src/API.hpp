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
    // API version 6. `value` is a pointer to an OpenRBRVRViewInfo that is filled with the view that is being rendered at the moment
    // (call it from a hook that runs while the game draws the 3D scene, for example inside RBR's scene render function).
    // Returns 0 on success, -1 if no VR view is being rendered or the struct is too small.
    GET_VIEW_INFO = 0x80,
};

// Filled by GET_VIEW_INFO. Plugins that post-process the 3D scene need the real VR projection of every view: it differs from the
// game's own projection (field of view, off-axis per eye, a narrower field of view for the quad view focus views).
struct OpenRBRVRViewInfo {
    uint32_t struct_size; // set by the caller to sizeof(OpenRBRVRViewInfo)
    // 1: the render target holds one view, 2: multiview rendering, the render target has two layers (the two eyes) that are drawn together
    uint32_t view_count;
    uint32_t focus_view; // 1 for the focus (inner, high density) views of quad view rendering, 0 for the normal / peripheral views
    uint32_t width; // render resolution of the view in pixels
    uint32_t height;
    // Projection matrices in the layout of D3DMATRIX (_11.._44, row by row), in the convention openRBRVR uses for the VR views:
    // right handed, the view looks towards -z (_34 == -1). [0] is the projection of the (left / first) view, [1] the second layer
    // when view_count is 2 and the same as [0] otherwise.
    float projection[2][16];
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
