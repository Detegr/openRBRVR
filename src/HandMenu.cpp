#include "HandMenu.hpp"
#include "HandDiagnostics.hpp"
#include <cmath>

using namespace HandMenuTuning;

bool HandMenuHold::update(bool touching, double now)
{
    if (!touching || !std::isfinite(now)) {
        *this = {};
        return false;
    }
    if (started < 0 || now < last_sample || now - last_sample > help_max_sample_gap)
        started = now;
    last_sample = now;
    if (!fired && now - started >= help_hold_seconds) {
        fired = true;
        return true;
    }
    return false;
}

float HandMenuHold::progress(double now) const
{
    if (fired)
        return 1;
    return started < 0 ? 0 : glm::clamp(static_cast<float>((now - started) / help_hold_seconds), 0.0f, 1.0f);
}

bool HandMenuPinch::update(bool valid, bool facing, float separation, double now)
{
    if (!valid || !std::isfinite(separation)) {
        reset();
        return false;
    }
    if (facing) {
        if (facing_since < 0)
            facing_since = now;
    } else {
        facing_since = -1;
    }
    const bool stable = facing_since >= 0 && now - facing_since >= palm_stable;
    if (separation >= pinch_off) {
        pinched = false;
        released = true;
    }
    bool event = false;
    if (!pinched && separation <= pinch_on) {
        pinched = true;
        event = stable && released && now >= next_activation;
        released = false; // Also consume pinches made before arming or during cooldown.
        if (event)
            next_activation = now + cooldown;
    }
    armed = stable && released && now >= next_activation;
    return event;
}

int HandMenuTouch::button(glm::vec3 tip, float margin)
{
    if (std::abs(tip.x) > button_half_width + margin)
        return -1;
    for (int i = 0; i < static_cast<int>(button_centers.size()); ++i)
        if (std::abs(tip.y - button_centers[i]) <= button_half_height + margin)
            return i;
    return -1;
}

int HandMenuTouch::update(bool valid, glm::vec3 tip)
{
    if (!valid || !std::isfinite(tip.x) || !std::isfinite(tip.y) || !std::isfinite(tip.z)) {
        reset();
        return -1;
    }
    hover = tip.z >= back_depth && tip.z <= hover_depth ? button(tip) : -1;
    if (tip.z >= release_depth) {
        pressed = -1;
        ready = true;
    } else if (pressed >= 0 && button(tip, release_margin) != pressed) {
        // Leaving the expanded rectangle cancels feedback but cannot rearm a
        // sideways slide. A withdrawal in front of the panel is still required.
        pressed = -1;
        ready = false;
    }
    int action = -1;
    if (ready && hover >= 0 && tip.z <= press_depth && tip.z >= back_depth) {
        if (previous_depth > press_depth) {
            action = pressed = hover;
        }
        ready = false;
    }
    previous_depth = tip.z;
    return action;
}

namespace {
    glm::vec3 position(const XrPosef& pose) { return { pose.position.x, pose.position.y, pose.position.z }; }
    glm::quat rotation(const XrPosef& pose) { return glm::normalize(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z)); }
    bool valid_pose(const XrPosef& pose)
    {
        const auto& q = pose.orientation;
        const float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        return std::isfinite(pose.position.x) && std::isfinite(pose.position.y) && std::isfinite(pose.position.z)
            && std::isfinite(norm) && norm > 0.5f && norm < 1.5f;
    }
    bool valid_hand(const HandTracking::Hand& hand)
    {
        constexpr auto flags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if (!hand.active)
            return false;
        for (const auto& joint : hand.joints)
            if ((joint.locationFlags & flags) != flags || !valid_pose(joint.pose))
                return false;
        return true;
    }
    M4 facing_pose(glm::vec3 center, const XrPosef& head)
    {
        const auto z = glm::normalize(position(head) - center);
        auto up = rotation(head) * glm::vec3(0, 1, 0);
        if (std::abs(glm::dot(up, z)) > 0.95f)
            up = rotation(head) * glm::vec3(1, 0, 0);
        const auto x = glm::normalize(glm::cross(up, z));
        const auto y = glm::cross(z, x);
        return M4(glm::vec4(x, 0), glm::vec4(y, 0), glm::vec4(z, 0), glm::vec4(center, 1));
    }
}

bool HandPlacedButton::update(const std::array<glm::vec3, 2>& tips)
{
    hover = false;
    for (const auto& tip : tips)
        if (!std::isfinite(tip.x) || !std::isfinite(tip.y) || !std::isfinite(tip.z)) {
            clear_contact();
            return false;
        }
    if (state == State::Placing) {
        pose[3] = glm::vec4(tips[1], 1);
        clear_contact();
        return false;
    }
    if (state != State::Locked) {
        clear_contact();
        return false;
    }
    const auto inverse = glm::inverse(pose);
    for (size_t i = 0; i < tips.size(); ++i) {
        const auto p = glm::abs(glm::vec3(inverse * glm::vec4(tips[i], 1)));
        const float distance = std::max(p.x, std::max(p.y, p.z));
        if (distance >= start_cube_release) {
            released[i] = true;
            contacts[i] = false;
        } else {
            hover = true;
        }
        if (distance <= start_cube_half && !contacts[i]) {
            contacts[i] = released[i];
            released[i] = false;
        }
    }
    // Multiple fingers hold one virtual button; release when the last leaves.
    return is_down();
}

void HandMenu::reset(bool clear_placed_buttons)
{
    if (open_)
        hand_log("Hand menu closed/reset");
    const auto start = start_button_, help = help_button_;
    *this = {};
    if (!clear_placed_buttons) {
        if (start.state == HandPlacedButton::State::Locked)
            start_button_ = start;
        if (help.state == HandPlacedButton::State::Locked)
            help_button_ = help;
        start_button_.clear_contact();
        help_button_.clear_contact();
    }
}

void HandMenu::restore_buttons(const std::array<std::optional<M4>, 2>& poses)
{
    reset(true);
    for (size_t i = 0; i < poses.size(); ++i) {
        if (!poses[i])
            continue;
        auto& cube = i == 0 ? start_button_ : help_button_;
        cube.state = HandPlacedButton::State::Locked;
        cube.pose = *poses[i];
        cube.clear_contact(); // Loading never presses or starts a help countdown.
    }
}

void HandMenu::rebase_after_recenter(const XrPosef& old_reference_in_new)
{
    if (!has_placement())
        return;
    if (!valid_pose(old_reference_in_new)) {
        reset(true);
        return;
    }
    auto transform = glm::mat4_cast(rotation(old_reference_in_new));
    transform[3] = glm::vec4(position(old_reference_in_new), 1);
    panel_pose_ = transform * panel_pose_;
    start_button_.pose = transform * start_button_.pose;
    help_button_.pose = transform * help_button_.pose;
    help_button_.clear_contact();
    help_feedback_until_ = 0;
    start_button_.clear_contact();
    touches_ = {};
    help_holds_ = {};
    help_progress_ = 0;
    pinch_.reset();
    icon_ = tips_valid_ = false;
    // Retain only the short button/color feedback. Fresh joint positions are
    // populated before drawing again; no old fingertip coordinates survive.
    tips_ = {};
    hand_log("Hand menu/placed cubes rebased after recenter; withdraw fingertips to rearm");
}

HandMenu::Action HandMenu::update(const std::array<HandTracking::Hand, 2>& hands, const XrPosef& head,
    bool eligible, double now)
{
    if (!eligible || !valid_pose(head)) {
        reset();
        return Action::None;
    }
    if (!valid_hand(hands[0]) || !valid_hand(hands[1])) {
        if (!tracking_suspended_)
            hand_log("Hand menu: hand tracking lost; retaining panel and placements, cancelling contact");
        tracking_suspended_ = true;
        active_ = icon_ = tips_valid_ = false;
        touches_ = {};
        help_holds_ = {};
        help_progress_ = 0;
        feedback_hand_ = feedback_button_ = -1;
        feedback_until_ = help_feedback_until_ = 0;
        start_button_.clear_contact();
        help_button_.clear_contact();
        pinch_.reset();
        return Action::None;
    }
    if (tracking_suspended_)
        hand_log("Hand menu: hand tracking recovered; withdraw fingertips/release pinch to rearm");
    tracking_suspended_ = false;
    active_ = true;
    const auto& palm = hands[0].joints[XR_HAND_JOINT_PALM_EXT].pose;
    const auto to_head = position(head) - position(palm);
    const float distance = glm::length(to_head);
    const bool in_range = distance >= min_distance && distance <= max_distance;
    // OpenXR joint convention: +Y points out of the BACK of both hands.
    // Thus -Y is the palm normal; use the already compensated joint rotation.
    const auto normal = rotation(palm) * glm::vec3(0, -1, 0);
    const float facing = in_range ? glm::dot(normal, to_head / distance) : 0;
    const bool oriented = facing > facing_cosine;
    const auto separation = glm::distance(position(hands[0].joints[XR_HAND_JOINT_THUMB_TIP_EXT].pose),
        position(hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose));
    const bool local_event = pinch_.update(in_range, oriented, separation, now);
    // Icon visibility describes the palm pose, independent of finger curl,
    // pinch release or cooldown. Activation still needs a released, armed pinch.
    const bool stable_palm = pinch_.facing_since >= 0 && now - pinch_.facing_since >= palm_stable;
    icon_ = !open_ && in_range && oriented && stable_palm;
    const auto hand_pose = in_range ? facing_pose(position(palm), head) : M4(1);
    icon_pose_ = hand_pose;
    // Beside the palm, clear of extended fingers and the glove surface.
    icon_pose_[3] += hand_pose[0] * icon_side_offset + hand_pose[1] * icon_up_offset + hand_pose[2] * icon_forward_offset;
    if (now >= feedback_until_)
        feedback_hand_ = feedback_button_ = -1;
    tips_valid_ = open_ || feedback_hand_ >= 0 || has_placement();
    for (size_t i = 0; i < hands.size(); ++i)
        tips_[i] = position(hands[i].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose);
    start_button_.update(tips_);
    help_button_.update(tips_);
    Action panel_action = Action::None;
    bool panel_handled = false;
    if (open_) {
        const auto inverse_panel = glm::inverse(panel_pose_);
        std::array<int, 2> buttons;
        for (size_t i = 0; i < hands.size(); ++i)
            buttons[i] = touches_[i].update(true, glm::vec3(inverse_panel * glm::vec4(tips_[i], 1)));
        for (size_t i = 0; i < hands.size(); ++i) {
            if (buttons[i] < 0)
                continue;
            if (buttons[i] != 1) {
                auto& cube = buttons[i] == 0 ? start_button_ : help_button_;
                auto& other = buttons[i] == 0 ? help_button_ : start_button_;
                const auto name = buttons[i] == 0 ? "Start engine" : "Call for help";
                // Only the left index locks the right-index placement.
                if (cube.state == HandPlacedButton::State::Placing && i != 0)
                    continue;
                if (cube.state == HandPlacedButton::State::Placing) {
                    cube.state = HandPlacedButton::State::Locked;
                    locked_button_ = buttons[i] == 0 ? 0 : 1;
                    hand_log(std::format("{} button locked; withdraw fingertips to rearm", name));
                } else {
                    cube.state = HandPlacedButton::State::Placing;
                    cube.pose = facing_pose(tips_[1], head);
                    if (other.state == HandPlacedButton::State::Placing)
                        other = {};
                    hand_log(std::format("{} placement: cube follows right index; left index presses its placement item to lock", name));
                }
                cube.clear_contact();
                if (buttons[i] == 2 || help_button_.state != HandPlacedButton::State::Locked) {
                    help_holds_ = {};
                    help_progress_ = 0;
                    help_feedback_until_ = 0;
                }
            } else {
                open_ = false;
                if (start_button_.state == HandPlacedButton::State::Placing)
                    start_button_ = {};
                if (help_button_.state == HandPlacedButton::State::Placing)
                    help_button_ = {};
                panel_action = Action::Close;
            }
            panel_handled = true;
            feedback_hand_ = static_cast<int>(i);
            feedback_button_ = buttons[i];
            feedback_until_ = now + touch_feedback;
            touches_ = {}; // Both hands withdraw; simultaneous touches produce one action.
            hand_log(std::format("Hand menu {} index touch: {}", i == 0 ? "left" : "right",
                buttons[i] == 1 ? "Close" : buttons[i] == 0 ? "Place start button"
                                                            : "Place call for help"));
            break;
        }
    }

    if (!panel_handled && in_range && local_event) {
        open_ = !open_;
        if (!open_ && start_button_.state == HandPlacedButton::State::Placing)
            start_button_ = {};
        if (!open_ && help_button_.state == HandPlacedButton::State::Placing)
            help_button_ = {};
        icon_ = false;
        touches_ = {};
        if (open_) {
            const auto center = position(palm) + glm::vec3(hand_pose[1]) * panel_offset + glm::vec3(hand_pose[2]) * panel_forward_offset;
            panel_pose_ = facing_pose(center, head); // Capture, never chase the hand.
        }
        hand_log(std::format("Hand menu {} via local palm pinch", open_ ? "opened" : "closed"));
    }
    help_progress_ = 0;
    for (size_t i = 0; i < hands.size(); ++i) {
        if (help_holds_[i].update(help_button_.contacts[i], now)) {
            help_feedback_until_ = now + touch_feedback;
            help_button_.clear_contact(); // Held fingers must withdraw before another countdown.
            help_holds_ = {};
            help_progress_ = 1;
            hand_log("Placed Call for help button: 2-second continuous touch");
            return Action::CallForHelp;
        }
        help_progress_ = std::max(help_progress_, help_holds_[i].progress(now));
    }
    if (now >= help_feedback_until_)
        help_feedback_until_ = 0;
    return panel_action;
}

namespace {
    void quad(HandMesh& mesh, const M4& pose, float x, float y, float w, float h, float z, D3DCOLOR color)
    {
        const auto base = static_cast<uint16_t>(mesh.vertices.size());
        for (const auto p : { glm::vec3(x, y, z), glm::vec3(x + w, y, z), glm::vec3(x + w, y + h, z), glm::vec3(x, y + h, z) })
            mesh.vertices.push_back({ glm::vec3(pose * glm::vec4(p, 1)), color, {} });
        for (const auto i : { 0, 1, 2, 0, 2, 3 })
            mesh.indices.push_back(base + i);
    }
    // Tiny embedded 5x7 font for the labels; no texture or UI dependency.
    const std::array<uint8_t, 7>& glyph(char c)
    {
        static const std::array<uint8_t, 7> r { 30, 17, 17, 30, 20, 18, 17 }, e { 31, 16, 16, 30, 16, 16, 31 },
            n { 17, 25, 25, 21, 19, 19, 17 }, t { 31, 4, 4, 4, 4, 4, 4 }, c_ { 14, 17, 16, 16, 16, 17, 14 },
            p { 30, 17, 17, 30, 16, 16, 16 }, a { 14, 17, 17, 31, 17, 17, 17 },
            b { 30, 17, 17, 30, 17, 17, 30 }, u { 17, 17, 17, 17, 17, 17, 14 },
            k { 17, 18, 20, 24, 20, 18, 17 }, f { 31, 16, 16, 30, 16, 16, 16 },
            h { 17, 17, 17, 31, 17, 17, 17 }, d { 30, 17, 17, 17, 17, 17, 30 },
            two { 14, 17, 1, 2, 4, 8, 31 }, space {},
            l { 16, 16, 16, 16, 16, 16, 31 }, o { 14, 17, 17, 17, 17, 17, 14 }, s { 15, 16, 16, 14, 1, 1, 30 };
        switch (c) {
            case 'P': return p;
            case 'A': return a;
            case 'B': return b;
            case 'U': return u;
            case 'K': return k;
            case 'F': return f;
            case 'H': return h;
            case 'D': return d;
            case '2': return two;
            case ' ': return space;
            case 'R': return r;
            case 'E': return e;
            case 'N': return n;
            case 'T': return t;
            case 'C': return c_;
            case 'L': return l;
            case 'O': return o;
            default: return s;
        }
    }
    void label(HandMesh& mesh, const M4& pose, std::string_view text, float y, float pixel = 0.0023f)
    {
        float x = -static_cast<float>(text.size() * 6 - 1) * pixel / 2;
        for (char c : text) {
            const auto& rows = glyph(c);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (rows[row] & (1 << (4 - col)))
                        quad(mesh, pose, x + col * pixel, y + (2.5f - row) * pixel, pixel, pixel, 0.002f, 0xfff5f5f5);
            x += pixel * 6;
        }
    }
}

void HandMenu::append_mesh(HandMesh& mesh) const
{
    if (!active_ && !tracking_suspended_)
        return;
    if (icon_) {
        // Circle with three horizontal strokes.
        constexpr int segments = 40;
        for (int i = 0; i < segments; ++i) {
            const float a = i * 6.2831853f / segments, b = (i + 1) * 6.2831853f / segments;
            const auto base = static_cast<uint16_t>(mesh.vertices.size());
            for (auto p : { glm::vec3(std::cos(a) * 0.022f, std::sin(a) * 0.022f, 0),
                     glm::vec3(std::cos(b) * 0.022f, std::sin(b) * 0.022f, 0),
                     glm::vec3(std::cos(b) * 0.019f, std::sin(b) * 0.019f, 0),
                     glm::vec3(std::cos(a) * 0.019f, std::sin(a) * 0.019f, 0) })
                mesh.vertices.push_back({ glm::vec3(icon_pose_ * glm::vec4(p, 1)), 0xfff5f5f5, {} });
            for (int index : { 0, 1, 2, 0, 2, 3 })
                mesh.indices.push_back(base + index);
        }
        for (float y : { -0.009f, -0.0015f, 0.006f })
            quad(mesh, icon_pose_, -0.011f, y, 0.022f, 0.003f, 0.001f, 0xfff5f5f5);
    }
    if (open_) {
        quad(mesh, panel_pose_, -panel_width / 2, -panel_height / 2 - button_y, panel_width, panel_height, -0.001f, 0xff20242b);
        for (int i = 0; i < static_cast<int>(button_centers.size()); ++i) {
            const float y = button_centers[i];
            const bool hover = touches_[0].hover == i || touches_[1].hover == i;
            const D3DCOLOR color = feedback_button_ == i ? 0xffffac42 : hover ? 0xffb44238
                                                                              : 0xff66302d;
            quad(mesh, panel_pose_, -button_half_width, y - button_half_height, 2 * button_half_width, 2 * button_half_height, 0, color);
            if (i == 0) {
                label(mesh, panel_pose_, "PLACE START", y + 0.008f, 0.0019f);
                label(mesh, panel_pose_, "BUTTON", y - 0.008f, 0.0019f);
            } else if (i == 2) {
                label(mesh, panel_pose_, "PLACE CALL", y + 0.008f, 0.0019f);
                label(mesh, panel_pose_, "FOR HELP", y - 0.008f, 0.0019f);
            } else {
                label(mesh, panel_pose_, "CLOSE", y);
            }
        }
    }
    for (const bool help : { false, true }) {
        const auto& cube = help ? help_button_ : start_button_;
        if (!open_ || cube.state == HandPlacedButton::State::Unplaced
            || (tracking_suspended_ && cube.state == HandPlacedButton::State::Placing))
            continue;
        const auto& pose = cube.pose;
        constexpr float h = start_cube_half;
        const D3DCOLOR color = cube.state == HandPlacedButton::State::Placing ? 0xff42bbed
            : (help ? help_feedback_until_ > 0 : cube.is_down())              ? 0xffffac42
            : cube.hover                                                      ? 0xff76e3ef
            : help                                                            ? 0xff684ba0
                                                                              : 0xff993e35;
        // Six faces, in the exact same local coordinates as the volume test.
        for (int axis = 0; axis < 3; ++axis)
            for (float side : { -1.0f, 1.0f }) {
                auto face = pose;
                face[0] = pose[(axis + 1) % 3];
                face[1] = pose[(axis + 2) % 3] * side;
                face[2] = pose[axis] * side;
                quad(mesh, face, -h, -h, 2 * h, 2 * h, h, color);
            }
        auto front = pose;
        front[3] += pose[2] * h;
        label(mesh, front, help ? "HELP" : "START", help ? 0.006f : 0, 0.0014f);
        if (help) {
            label(mesh, front, "HOLD 2S", -0.006f, 0.0009f);
            quad(mesh, front, -0.02f, -0.019f, 0.04f, 0.003f, 0.003f, 0xff20242b);
            if (help_progress_ > 0)
                quad(mesh, front, -0.02f, -0.019f, 0.04f * help_progress_, 0.003f, 0.004f, 0xff76e3ef);
        }
    }
    if (open_ && tips_valid_) {
        // Small cross at the actual fingertip, facing the panel. Flash on touch.
        for (size_t i = 0; i < tips_.size(); ++i) {
            auto pose = panel_pose_;
            pose[3] = glm::vec4(tips_[i], 1);
            const D3DCOLOR color = feedback_hand_ == static_cast<int>(i) ? 0xffffac42 : touches_[i].hover >= 0 ? 0xff76e3ef
                                                                                                               : 0xffeeeeee;
            quad(mesh, pose, -0.005f, -0.0015f, 0.01f, 0.003f, 0, color);
            quad(mesh, pose, -0.0015f, -0.005f, 0.003f, 0.01f, 0, color);
        }
    }
}
