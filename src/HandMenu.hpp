#pragma once

#include "HandMesh.hpp"
#include <optional>
#include <string_view>

// Metres and seconds. Keep tuning values together for headset validation.
namespace HandMenuTuning {
    constexpr float min_distance = 0.18f, max_distance = 0.75f;
    constexpr float facing_cosine = 0.35f; // Allow a natural, tilted open palm (~70 degrees).
    constexpr double palm_stable = 0.25, cooldown = 0.55;
    constexpr float pinch_on = 0.022f, pinch_off = 0.038f;
    constexpr float panel_width = 0.18f, panel_height = 0.174f;
    constexpr float button_half_width = 0.08f, button_half_height = 0.021f;
    constexpr float button_y = 0.027f;
    constexpr std::array<float, 3> button_centers { button_y, -button_y, -3 * button_y };
    constexpr double help_hold_seconds = 2.0;
    constexpr double help_max_sample_gap = 0.25;
    constexpr float start_cube_half = 0.025f, start_cube_release = 0.032f;
    constexpr float press_depth = 0.008f, back_depth = -0.018f;
    constexpr float hover_depth = 0.06f, release_depth = 0.035f, release_margin = 0.012f;
    constexpr float panel_offset = 0.214f, panel_forward_offset = 0.03f;
    constexpr float icon_side_offset = -0.10f, icon_up_offset = 0.025f, icon_forward_offset = 0.07f;
    constexpr double touch_feedback = 0.18;
}

struct HandMenuPinch {
    bool update(bool valid, bool facing, float separation, double now);
    void reset() { *this = {}; }
    bool armed = false;
    bool released = false, pinched = false;
    double facing_since = -1, next_activation = 0;
};

struct HandMenuTouch {
    // -1: no entry, 0: Place start button, 1: Close, 2: Place call for help.
    int update(bool valid, glm::vec3 tip);
    void reset() { *this = {}; }
    static int button(glm::vec3 tip, float margin = 0);
    int hover = -1, pressed = -1;
    bool ready = false;
    float previous_depth = HandMenuTuning::back_depth;
};

struct HandMenuHold {
    double started = -1;
    double last_sample = -1;
    bool fired = false;
    bool update(bool touching, double now);
    float progress(double now) const;
};

struct HandPlacedButton {
    enum class State { Unplaced,
        Placing,
        Locked };
    State state = State::Unplaced;
    M4 pose { 1 };
    std::array<bool, 2> released {};
    std::array<bool, 2> contacts {};
    bool hover = false;
    void clear_contact()
    {
        released = {};
        contacts = {};
        hover = false;
    }
    bool is_down() const { return contacts[0] || contacts[1]; }
    bool update(const std::array<glm::vec3, 2>& tips);
};

class HandMenu {
public:
    enum class Action { None,
        Close,
        CallForHelp };
    Action update(const std::array<HandTracking::Hand, 2>& hands, const XrPosef& head,
        bool eligible, double now);
    void reset(bool clear_placed_buttons = false);
    void restore_buttons(const std::array<std::optional<M4>, 2>& poses);
    int take_locked_button()
    {
        const int button = locked_button_;
        locked_button_ = -1;
        return button;
    }
    // Keep an open panel at the same physical pose after our recenter changes
    // reference coordinates. Clear both touch states and require fresh release.
    void rebase_after_recenter(const XrPosef& old_reference_in_new);
    void append_mesh(HandMesh& mesh) const;
    bool is_open() const { return open_; }
    bool is_active() const { return active_; }
    const M4& panel_pose() const { return panel_pose_; }
    const HandPlacedButton& start_button() const { return start_button_; }
    const HandPlacedButton& help_button() const { return help_button_; }
    bool has_placement() const { return open_ || start_button_.state != HandPlacedButton::State::Unplaced || help_button_.state != HandPlacedButton::State::Unplaced; }

private:
    HandMenuPinch pinch_;
    std::array<HandMenuTouch, 2> touches_;
    std::array<HandMenuHold, 2> help_holds_;
    float help_progress_ = 0;
    bool open_ = false, icon_ = false;
    bool tips_valid_ = false;
    M4 panel_pose_ { 1 }, icon_pose_ { 1 };
    std::array<glm::vec3, 2> tips_ {};
    int feedback_hand_ = -1, feedback_button_ = -1;
    double feedback_until_ = 0;
    HandPlacedButton start_button_, help_button_;
    double help_feedback_until_ = 0;
    bool active_ = false, tracking_suspended_ = false;
    int locked_button_ = -1;
};
