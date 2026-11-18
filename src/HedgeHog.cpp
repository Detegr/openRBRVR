#include "HedgeHog.hpp"
#include "Globals.hpp"
#include "RBR.hpp"

#include <algorithm>
#include <cmath>
#include <gtc/matrix_access.hpp>
#include <optional>

namespace hedgehog {
    static constexpr uint32_t MAIN_VIEW_SLOT = 0;

    static bool culling_enabled = true;

    void __stdcall hedgehog_cull(VisibilityManager* visibility_manager, uint32_t slot)
    {
        if (slot == MAIN_VIEW_SLOT && !culling_enabled) {
            return;
        }
        g::hooks::hedgehog_cull.call(visibility_manager, slot);
    }

    void enable_hedgehog_cull(bool enabled)
    {
        culling_enabled = enabled;
    }

    static CHH_CameraFreeFly* get_cull_camera(CHH_World* world)
    {
        if (!world->culling_enabled || !world->visibility_manager)
            return nullptr;

        // HedgeHog culls with either camera 0 or 1
        const auto camera_idx = std::max<uint32_t>(world->active_camera, 1);
        return world->cameras_begin[camera_idx];
    }

    static Plane plane_from_rows(const M4& m, int row, float sign)
    {
        return glm::row(m, 3) + sign * glm::row(m, row);
    }

    static const PlaneSet& make_world_planes(CHH_CameraFreeFly* cam, const M4& m)
    {
        constexpr Plane inside { 0.0f, 0.0f, 0.0f, 1.0f };
        const auto far_z = cam->far_z;
        const auto has_far = std::isfinite(far_z) && far_z > 0.0f;

        Plane* p = cam->view_planes.set.planes;
        p[0] = plane_from_rows(m, 0, +1.0f); // left
        p[1] = plane_from_rows(m, 0, -1.0f); // right
        p[2] = plane_from_rows(m, 1, -1.0f); // top
        p[3] = plane_from_rows(m, 1, +1.0f); // bottom
        p[4] = has_far ? Plane { 0.0f, 0.0f, 0.0f, far_z } - glm::row(m, 3) : inside; // far
        p[5] = glm::row(m, 2); // near

        cam->vtbl->to_world_space(cam);
        return cam->world_planes.set;
    }

    static BoxTestResult box_test(const BoundingBox* box, const PlaneSet* set)
    {
        // Same as HedgeHog's box test at 0x100331c0

        const auto center = glm::make_vec3(box->center);
        const auto half_size = glm::make_vec3(box->half_size);
        const auto min = center - half_size;
        const auto max = center + half_size;

        auto result = BoxTestResult::Inside;
        for (const auto& plane : set->planes) {
            const auto normal = glm::vec3(plane);
            const auto negative = glm::lessThan(normal, glm::vec3(0.0f));

            // The box corners furthest along and against the plane normal
            const auto furthest = glm::mix(max, min, negative);
            const auto nearest = glm::mix(min, max, negative);

            if (glm::dot(normal, furthest) + plane.w < 0.0f) {
                return BoxTestResult::Outside;
            }
            if (glm::dot(normal, nearest) + plane.w <= 0.0f) {
                result = BoxTestResult::PartlyInside;
            }
        }

        return result;
    }

    static BoxTestResult box_test_multiple(const BoundingBox* box, std::span<const PlaneSet> sets)
    {
        auto best = BoxTestResult::Outside;
        for (const auto& set : sets) {
            best = std::min(best, box_test(box, &set));
            if (best == BoxTestResult::Inside)
                break;
        }
        return best;
    }

    void cull(void* p, const M4& clip_matrix)
    {
        auto* world = static_cast<CHH_World*>(p);
        if (auto cam = get_cull_camera(world); cam) {
            make_world_planes(cam, clip_matrix);
            world->visibility_manager->data->main_view_planes = cam->world_planes;
            g::hooks::hedgehog_cull.call(world->visibility_manager, MAIN_VIEW_SLOT);
        }
    }

    // Cull function that calls HedgeHog's box test for multiple planes and combines the results.
    // Tests all the objects, or only the given ones.
    static void cull_objects(void* p, std::span<const M4> clip_matrices, std::optional<std::span<const int32_t>> objects)
    {
        auto* world = static_cast<CHH_World*>(p);
        auto cam = get_cull_camera(world);
        if (!cam) {
            return;
        }

        constexpr size_t max_views = 2;
        PlaneSet sets[max_views];
        const auto count = std::min(clip_matrices.size(), max_views);
        for (size_t i = 0; i < count; ++i) {
            sets[i] = make_world_planes(cam, clip_matrices[i]);
        }
        const auto planes = std::span<const PlaneSet>(sets, count);

        auto vis = world->visibility_manager->data;
        if (!vis->objects_begin) {
            vis->visible_object_count = 0;
            vis->visible_subitem_count = 0;
            return;
        }

        const auto object_count = static_cast<int32_t>(vis->objects_end - vis->objects_begin);

        if (objects) {
            // Only the given objects are tested, so hide the ones the previous cull left visible
            for (const auto i : std::span(vis->visible_object_indices, vis->visible_object_count)) {
                vis->objects_begin[i]->object->visible = 0;
            }
        }

        vis->visible_object_count = 0;
        vis->visible_subitem_count = 0;

        const auto test_object = [&](int32_t i) {
            Object* obj = vis->objects_begin[i]->object;

            const auto hit = box_test_multiple(&obj->box, planes);
            obj->visible = (hit != BoxTestResult::Outside);
            vis->box_test_results[i] = hit;
            if (hit == BoxTestResult::Outside)
                return;

            vis->visible_object_indices[vis->visible_object_count] = i;
            vis->visible_object_count++;

            for (SubItem* sub : std::span(obj->sub_items_begin, obj->sub_items_end)) {
                const auto hit = box_test_multiple(&sub->box, planes);
                vis->subitem_box_test_results[sub->id].box_test_result = hit;

                if (hit == BoxTestResult::Outside)
                    continue;

                vis->visible_subitem_ids[vis->visible_subitem_count] = sub->id;
                vis->visible_subitem_count++;
            }
        };

        if (objects) {
            for (const auto i : *objects) {
                if (i >= 0 && i < object_count) {
                    test_object(i);
                }
            }
        } else {
            for (int32_t i = 0; i < object_count; ++i) {
                test_object(i);
            }
        }
    }

    // Used to cull both eye projections with one draw call (when multiview is enabled)
    void cull_union(void* world, std::span<const M4> clip_matrices)
    {
        cull_objects(world, clip_matrices, std::nullopt);
    }

    void cull_subset(void* world, std::span<const M4> clip_matrices, std::span<const int32_t> objects)
    {
        cull_objects(world, clip_matrices, objects);
    }

    std::span<const int32_t> visible_objects(void* p)
    {
        auto* world = static_cast<CHH_World*>(p);
        if (!get_cull_camera(world)) {
            return { };
        }
        auto vis = world->visibility_manager->data;
        return std::span<const int32_t>(vis->visible_object_indices, vis->visible_object_count);
    }
}
