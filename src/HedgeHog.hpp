#pragma once

#include "Util.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace hedgehog {
    struct VisibilityManager;

    // Hookable functions
    void __stdcall hedgehog_cull(VisibilityManager* visibility_manager, uint32_t slot);

    // HedgeHog's own main view culling, done once per frame with the game camera
    void enable_hedgehog_cull(bool enabled);

    void cull(void* world, const M4& clip_matrix);
    void cull_union(void* world, std::span<const M4> clip_matrices);

    // Culls testing only the given objects (indices to the object list)
    void cull_subset(void* world, std::span<const M4> clip_matrices, std::span<const int32_t> objects);

    // Indices of the objects that were visible in the latest cull
    std::span<const int32_t> visible_objects(void* world);

#pragma pack(push, 1) // the offsets below are exact; don't let the compiler add padding

    using Plane = glm::vec4;

    struct PlaneSet {
        Plane planes[6]; // left, right, top, bottom, far, near
    };

    struct PlaneBlock { // the 100-byte block that HedgeHog copies
        int32_t count; // 6
        PlaneSet set;
    };

    struct BoundingBox {
        uint8_t _pad0[0x0c];
        float center[3]; // +0x0c
        float half_size[3]; // +0x18
    };

    struct SubItem {
        uint8_t _pad0[0x48];
        int32_t id; // 0x48
        BoundingBox box; // 0x4c
    };

    enum class BoxTestResult : uint32_t {
        Inside = 0,
        PartlyInside = 1,
        Outside = 2,
    };

    struct SubResult {
        BoxTestResult box_test_result;
        SubItem* item;
    };

    struct CHH_CameraFreeFly;

    struct CameraVtbl {
        void* UNUSED[6];
        void(__thiscall* to_world_space)(CHH_CameraFreeFly* self); // slot 6 (0x10013b20)
    };

    struct CHH_CameraFreeFly {
        const CameraVtbl* vtbl; // 0x000 (0x101af9ec)
        uint8_t _pad0[0x0e8 - 0x004];
        float view[16]; // 0x0e8
        float projection[16]; // 0x128 (HedgeHog's own 4:3 projection)
        float fovy; // 0x168
        float near_z; // 0x16c
        float far_z; // 0x170
        uint8_t _pad1[0x184 - 0x174];
        PlaneBlock view_planes; // 0x184 (planes at 0x188)
        PlaneBlock world_planes; // 0x1e8 (planes at 0x1ec)
    };

    struct Object {
        uint8_t _pad0[0x04];
        SubItem** sub_items_begin; // 0x04
        SubItem** sub_items_end; // 0x08
        uint8_t _pad1[0x1c - 0x0c];
        BoundingBox box; // 0x1c
        uint8_t _pad2[0xc8 - 0x40];
        uint8_t visible; // 0xc8
    };

    struct ObjectHandle {
        Object* object; // entries in the object list point to this
    };

    struct VisibilityData {
        PlaneBlock main_view_planes; // 0x000
        PlaneBlock cube_map_planes; // 0x064
        uint8_t _pad0[0x194 - 0x0c8];
        ObjectHandle** objects_begin; // 0x194
        ObjectHandle** objects_end; // 0x198
        uint8_t _pad1[0x1a0 - 0x19c];
        int32_t visible_subitem_count; // 0x1a0
        uint8_t _pad2[0x1b4 - 0x1a4];
        SubResult* subitem_box_test_results; // 0x1b4, indexed by sub-item ID
        uint8_t _pad3[0x1f4 - 0x1b8];
        int32_t* visible_subitem_ids; // 0x1f4
        uint8_t _pad4[0x230 - 0x1f8];
        int32_t visible_object_count; // 0x230
        uint8_t _pad5[0x244 - 0x234];
        BoxTestResult* box_test_results; // 0x244, indexed by object index
        uint8_t _pad6[0x284 - 0x248];
        int32_t* visible_object_indices; // 0x284
    };

    struct VisibilityManager {
        VisibilityData* data; // 0x000
    };

    struct CHH_World {
        void* vtbl; // 0x000
        uint8_t _pad0[0x160 - 0x004];
        uint32_t active_camera; // 0x160
        uint8_t _pad1[0x178 - 0x164];
        CHH_CameraFreeFly** cameras_begin; // 0x178
        CHH_CameraFreeFly** cameras_end; // 0x17c
        uint8_t _pad2[0x3a0 - 0x180];
        VisibilityManager* visibility_manager; // 0x3a0
        uint8_t _pad3[0x3c8 - 0x3a4];
        uint8_t culling_enabled; // 0x3c8
    };

#pragma pack(pop)

    static_assert(sizeof(Plane) == 16);
    static_assert(sizeof(PlaneBlock) == 100);
    static_assert(offsetof(BoundingBox, center) == 0x0c);
    static_assert(offsetof(BoundingBox, half_size) == 0x18);
    static_assert(offsetof(CameraVtbl, to_world_space) == 6 * sizeof(void*));
    static_assert(offsetof(CHH_CameraFreeFly, view) == 0x0e8);
    static_assert(offsetof(CHH_CameraFreeFly, projection) == 0x128);
    static_assert(offsetof(CHH_CameraFreeFly, near_z) == 0x16c);
    static_assert(offsetof(CHH_CameraFreeFly, view_planes) == 0x184);
    static_assert(offsetof(CHH_CameraFreeFly, world_planes) == 0x1e8);
    static_assert(offsetof(SubItem, id) == 0x48);
    static_assert(offsetof(SubItem, box) == 0x4c);
    static_assert(offsetof(Object, sub_items_begin) == 0x04);
    static_assert(offsetof(Object, box) == 0x1c);
    static_assert(offsetof(Object, visible) == 0xc8);
    static_assert(offsetof(VisibilityData, objects_begin) == 0x194);
    static_assert(offsetof(VisibilityData, visible_subitem_count) == 0x1a0);
    static_assert(offsetof(VisibilityData, subitem_box_test_results) == 0x1b4);
    static_assert(offsetof(VisibilityData, visible_subitem_ids) == 0x1f4);
    static_assert(offsetof(VisibilityData, visible_object_count) == 0x230);
    static_assert(offsetof(VisibilityData, box_test_results) == 0x244);
    static_assert(offsetof(VisibilityData, visible_object_indices) == 0x284);
    static_assert(offsetof(CHH_World, active_camera) == 0x160);
    static_assert(offsetof(CHH_World, cameras_begin) == 0x178);
    static_assert(offsetof(CHH_World, visibility_manager) == 0x3a0);
    static_assert(offsetof(CHH_World, culling_enabled) == 0x3c8);
}
