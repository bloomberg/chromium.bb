// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_move.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace arc {
namespace input_overlay {
namespace {
// About Json strings.
constexpr char kKeys[] = "keys";
constexpr char kTargetArea[] = "target_area";
constexpr char kTopLeft[] = "top_left";
constexpr char kBottomRight[] = "bottom_right";

constexpr int kAxisSize = 2;
constexpr int kDirection[kActionMoveKeysSize][kAxisSize] = {{0, -1},
                                                            {-1, 0},
                                                            {0, 1},
                                                            {1, 0}};
// UI specs.
// Offset by label center.
constexpr int kLabelOffset = 49;

std::unique_ptr<Position> ParseApplyAreaPosition(const base::Value& value,
                                                 base::StringPiece key) {
  auto* point = value.FindDictKey(key);
  if (!point) {
    LOG(ERROR) << "Apply area in mouse move action requires: " << key;
    return nullptr;
  }
  auto pos = ParsePosition(*point);
  if (!pos) {
    LOG(ERROR) << "Not valid position for: " << key;
    return nullptr;
  }
  return pos;
}

}  // namespace

class ActionMove::ActionMoveMouseView : public ActionView {
 public:
  ActionMoveMouseView(Action* action,
                      DisplayOverlayController* display_overlay_controller,
                      const gfx::RectF& content_bounds)
      : ActionView(action, display_overlay_controller) {
    auto label = std::make_unique<ActionLabel>(u"mouse cursor lock (esc)");
    label->set_editable(false);
    auto label_size = label->GetPreferredSize();
    label->SetSize(label_size);
    SetSize(label_size);
    center_.set_x(label_size.width() / 2);
    center_.set_y(label_size.height() / 2);
    labels_.emplace_back(AddChildView(std::move(label)));
  }

  ActionMoveMouseView(const ActionMoveMouseView&) = delete;
  ActionMoveMouseView& operator=(const ActionMoveMouseView&) = delete;
  ~ActionMoveMouseView() override = default;
};

class ActionMove::ActionMoveKeyView : public ActionView {
 public:
  ActionMoveKeyView(Action* action,
                    DisplayOverlayController* display_overlay_controller,
                    const gfx::RectF& content_bounds)
      : ActionView(action, display_overlay_controller) {
    int radius =
        std::max(kActionMoveMinRadius, action->GetUIRadius(content_bounds));
    auto* action_move = static_cast<ActionMove*>(action);
    action_move->set_move_distance(radius / 2);
    SetSize(gfx::Size(radius * 2, radius * 2));
    auto circle = std::make_unique<ActionCircle>(radius);
    circle->SetPosition(gfx::Point());
    center_.set_x(radius);
    center_.set_y(radius);
    circle_ = AddChildView(std::move(circle));
    auto keys = action->current_binding()->keys();
    for (int i = 0; i < keys.size(); i++) {
      auto text = GetDisplayText(keys[i]);
      auto label = std::make_unique<ActionLabel>(base::UTF8ToUTF16(text));
      label->set_editable(true);
      auto label_size = label->GetPreferredSize();
      label->SetSize(label_size);
      int x = kDirection[i][0];
      int y = kDirection[i][1];
      auto pos = gfx::Point(
          radius + x * (radius - kLabelOffset) - label_size.width() / 2,
          radius + y * (radius - kLabelOffset) - label_size.height() / 2);
      label->SetPosition(pos);
      labels_.emplace_back(AddChildView(std::move(label)));
    }
  }

  ActionMoveKeyView(const ActionMoveKeyView&) = delete;
  ActionMoveKeyView& operator=(const ActionMoveKeyView&) = delete;
  ~ActionMoveKeyView() override = default;
};

ActionMove::ActionMove(aura::Window* window) : Action(window) {}

ActionMove::~ActionMove() = default;

bool ActionMove::ParseFromJson(const base::Value& value) {
  Action::ParseFromJson(value);
  if (parsed_input_sources_ == InputSource::IS_KEYBOARD) {
    if (locations_.size() == 0) {
      LOG(ERROR) << "Require at least one location for key-bound move action: "
                 << name_ << ".";
      return false;
    }
    return ParseJsonFromKeyboard(value);
  } else {
    return ParseJsonFromMouse(value);
  }
}

bool ActionMove::ParseJsonFromKeyboard(const base::Value& value) {
  auto* keys = value.FindListKey(kKeys);
  if (!keys) {
    LOG(ERROR) << "Require key codes for move key action: " << name_ << ".";
    return false;
  }
  if (keys->GetListDeprecated().size() != kActionMoveKeysSize) {
    LOG(ERROR) << "Not right amount of keys for action move keys. Require {"
               << kActionMoveKeysSize << "} keys, but got {"
               << keys->GetListDeprecated().size() << "} keys.";
    return false;
  }
  std::vector<ui::DomCode> keycodes;
  for (const base::Value& val : keys->GetListDeprecated()) {
    DCHECK(val.is_string());
    auto key = ui::KeycodeConverter::CodeStringToDomCode(val.GetString());
    if (key == ui::DomCode::NONE) {
      LOG(ERROR) << "Key code is invalid for move key action: " << name_
                 << ". It should be similar to {KeyA}, but got {" << val
                 << "}.";
      return false;
    }
    auto it = std::find(keycodes.begin(), keycodes.end(), key);
    if (it != keycodes.end()) {
      LOG(ERROR) << "Duplicated key {" << val
                 << "} for move key action: " << name_;
      return false;
    }
    keycodes.emplace_back(key);
  }
  original_binding_ = InputElement::CreateActionMoveKeyElement(keycodes);
  current_binding_ = InputElement::CreateActionMoveKeyElement(keycodes);

  return true;
}

bool ActionMove::ParseJsonFromMouse(const base::Value& value) {
  const auto* mouse_action = value.FindStringKey(kMouseAction);
  if (!mouse_action) {
    LOG(ERROR) << "Must include mouse action for mouse-bound move action.";
    return false;
  }
  if (*mouse_action != kHoverMove && *mouse_action != kPrimaryDragMove &&
      *mouse_action != kSecondaryDragMove) {
    LOG(ERROR) << "Not supported mouse action {" << mouse_action << "}.";
    return false;
  }
  require_mouse_locked_ = true;
  original_binding_ = InputElement::CreateActionMoveMouseElement(*mouse_action);
  current_binding_ = InputElement::CreateActionMoveMouseElement(*mouse_action);

  auto* target_area = value.FindDictKey(kTargetArea);
  if (target_area) {
    auto top_left = ParseApplyAreaPosition(*target_area, kTopLeft);
    if (!top_left)
      return false;
    auto bottom_right = ParseApplyAreaPosition(*target_area, kBottomRight);
    if (!bottom_right)
      return false;

    // Verify |top_left| is located on the top-left of the |bottom_right|. Use a
    // random positive window content bounds to test it.
    auto temp_rect = gfx::RectF(10, 10, 100, 100);
    auto top_left_point = top_left->CalculatePosition(temp_rect);
    auto bottom_right_point = bottom_right->CalculatePosition(temp_rect);
    if (top_left_point.x() > bottom_right_point.x() - 1 ||
        top_left_point.y() > bottom_right_point.y() - 1) {
      LOG(ERROR) << "Apply area in mouse move action is not verified. For "
                    "window content bounds "
                 << temp_rect.ToString() << ", get top-left position "
                 << top_left_point.ToString() << " and bottom-right position "
                 << bottom_right_point.ToString()
                 << ". Top-left position should be on the top-left of the "
                    "bottom-right position.";
      return false;
    }

    target_area_.emplace_back(std::move(top_left));
    target_area_.emplace_back(std::move(bottom_right));
  }

  return true;
}

bool ActionMove::RewriteEvent(const ui::Event& origin,
                              const gfx::RectF& content_bounds,
                              const bool is_mouse_locked,
                              std::list<ui::TouchEvent>& touch_events,
                              bool& keep_original_event) {
  if (IsNoneBound() || (IsKeyboardBound() && !origin.IsKeyEvent()) ||
      (IsMouseBound() && !origin.IsMouseEvent()))
    return false;
  DCHECK((IsKeyboardBound() && !IsMouseBound()) ||
         (!IsKeyboardBound() && IsMouseBound()));
  LogEvent(origin);

  // Rewrite for key event.
  if (IsKeyboardBound()) {
    auto* key_event = origin.AsKeyEvent();
    bool rewritten = RewriteKeyEvent(key_event, content_bounds, touch_events);
    LogTouchEvents(touch_events);
    return rewritten;
  }

  // Rewrite for mouse event.
  if (!is_mouse_locked)
    return false;
  auto* mouse_event = origin.AsMouseEvent();
  auto rewritten = RewriteMouseEvent(mouse_event, content_bounds, touch_events);
  LogTouchEvents(touch_events);

  return rewritten;
}

gfx::PointF ActionMove::GetUICenterPosition(const gfx::RectF& content_bounds) {
  if (locations().empty()) {
    DCHECK(IsMouseBound());
    return gfx::PointF(content_bounds.width() / 2, content_bounds.height() / 2);
  }
  auto* position = locations().front().get();
  return position->CalculatePosition(content_bounds);
}

std::unique_ptr<ActionView> ActionMove::CreateView(
    DisplayOverlayController* display_overlay_controller,
    const gfx::RectF& content_bounds) {
  std::unique_ptr<ActionView> view;
  if (IsMouseBound()) {
    view = std::make_unique<ActionMoveMouseView>(
        this, display_overlay_controller, content_bounds);
  } else {
    view = std::make_unique<ActionMoveKeyView>(this, display_overlay_controller,
                                               content_bounds);
  }
  view->set_editable(false);
  auto center_pos = GetUICenterPosition(content_bounds);
  view->SetPositionFromCenterPosition(center_pos);
  return view;
}

bool ActionMove::RewriteKeyEvent(const ui::KeyEvent* key_event,
                                 const gfx::RectF& content_bounds,
                                 std::list<ui::TouchEvent>& rewritten_events) {
  auto keys = current_binding_->keys();
  auto it = std::find(keys.begin(), keys.end(), key_event->code());
  if (it == keys.end())
    return false;

  // Ignore repeated key events, but consider it as processed.
  if (IsRepeatedKeyEvent(*key_event))
    return true;

  int index = it - keys.begin();
  DCHECK(index >= 0 && index < kActionMoveKeysSize);

  auto pos = CalculateTouchPosition(content_bounds);
  DCHECK(pos);

  if (key_event->type() == ui::ET_KEY_PRESSED) {
    if (!touch_id_) {
      // First key press generates touch press.
      touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
      last_touch_root_location_ = *pos;
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_PRESSED, last_touch_root_location_,
          last_touch_root_location_, key_event->time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()))
          .set_target(target_window_);
    }
    DCHECK(touch_id_);

    // Generate touch move.
    CalculateMoveVector(*pos, index, /* key_press */ true, content_bounds);
    rewritten_events.emplace_back(ui::TouchEvent(
        ui::EventType::ET_TOUCH_MOVED, last_touch_root_location_,
        last_touch_root_location_, key_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
    ui::Event::DispatcherApi(&(rewritten_events.back()))
        .set_target(target_window_);
    keys_pressed_.emplace(key_event->code());
  } else {
    if (keys_pressed_.size() > 1) {
      // Generate new move.
      CalculateMoveVector(*pos, index, /* key_press */ false, content_bounds);
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_MOVED, last_touch_root_location_,
          last_touch_root_location_, key_event->time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()))
          .set_target(target_window_);
    } else {
      // Generate touch release.
      rewritten_events.emplace_back(ui::TouchEvent(
          ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
          last_touch_root_location_, key_event->time_stamp(),
          ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_)));
      ui::Event::DispatcherApi(&(rewritten_events.back()))
          .set_target(target_window_);
      OnTouchReleased();
      move_vector_.set_x(0);
      move_vector_.set_y(0);
    }
    keys_pressed_.erase(key_event->code());
  }
  return true;
}

bool ActionMove::RewriteMouseEvent(
    const ui::MouseEvent* mouse_event,
    const gfx::RectF& content_bounds,
    std::list<ui::TouchEvent>& rewritten_events) {
  DCHECK(mouse_event);

  auto type = mouse_event->type();
  if (!current_binding_->mouse_types().contains(type) ||
      current_binding_->mouse_flags() != mouse_event->flags()) {
    return false;
  }

  auto mouse_location = gfx::Point(mouse_event->root_location());
  target_window_->GetHost()->ConvertPixelsToDIP(&mouse_location);
  auto mouse_location_f = gfx::PointF(mouse_location);
  // Discard mouse events outside of the app content bounds if the mouse is
  // locked.
  if (!content_bounds.Contains(mouse_location_f))
    return true;

  last_touch_root_location_ =
      TransformLocationInPixels(content_bounds, mouse_location_f);

  if (type == ui::ET_MOUSE_ENTERED || type == ui::ET_MOUSE_PRESSED)
    DCHECK(!touch_id_);
  // Mouse might be unlocked before ui::ET_MOUSE_EXITED, so no need to check
  // ui::ET_MOUSE_EXITED.
  if (type == ui::ET_MOUSE_RELEASED)
    DCHECK(touch_id_);
  if (!touch_id_) {
    touch_id_ = TouchIdManager::GetInstance()->ObtainTouchID();
    auto touch_down_pos = CalculateTouchPosition(content_bounds);
    if (touch_down_pos)
      last_touch_root_location_ = *touch_down_pos;
    rewritten_events.emplace_back(
        ui::EventType::ET_TOUCH_PRESSED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_));
  } else if (type == ui::ET_MOUSE_EXITED || type == ui::ET_MOUSE_RELEASED) {
    rewritten_events.emplace_back(
        ui::EventType::ET_TOUCH_RELEASED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_));
    OnTouchReleased();
  } else {
    DCHECK(type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED);
    rewritten_events.emplace_back(
        ui::EventType::ET_TOUCH_MOVED, last_touch_root_location_,
        last_touch_root_location_, mouse_event->time_stamp(),
        ui::PointerDetails(ui::EventPointerType::kTouch, *touch_id_));
  }
  ui::Event::DispatcherApi(&(rewritten_events.back()))
      .set_target(target_window_);
  return true;
}

void ActionMove::CalculateMoveVector(gfx::PointF& touch_press_pos,
                                     int direction_index,
                                     bool key_press,
                                     const gfx::RectF& content_bounds) {
  DCHECK(direction_index >= 0 && direction_index < kActionMoveKeysSize);
  auto new_move = gfx::Vector2dF(kDirection[direction_index][0],
                                 kDirection[direction_index][1]);
  float display_scale_factor = target_window_->GetHost()->device_scale_factor();
  float scale = display_scale_factor * move_distance_;
  new_move.Scale(scale, scale);
  if (key_press) {
    move_vector_ += new_move;
  } else {
    move_vector_ -= new_move;
  }
  last_touch_root_location_ = touch_press_pos + move_vector_;
  float x = last_touch_root_location_.x();
  float y = last_touch_root_location_.y();
  last_touch_root_location_.set_x(
      base::clamp(x, content_bounds.x() * display_scale_factor,
                  content_bounds.right() * display_scale_factor));
  last_touch_root_location_.set_y(
      base::clamp(y, content_bounds.y() * display_scale_factor,
                  content_bounds.bottom() * display_scale_factor));
}

absl::optional<gfx::RectF> ActionMove::CalculateApplyArea(
    const gfx::RectF& content_bounds) {
  if (target_area_.size() != 2)
    return absl::nullopt;

  auto top_left = target_area_[0]->CalculatePosition(content_bounds);
  auto bottom_right = target_area_[1]->CalculatePosition(content_bounds);
  return absl::make_optional<gfx::RectF>(
      top_left.x() + content_bounds.x(), top_left.y() + content_bounds.y(),
      bottom_right.x() - top_left.x(), bottom_right.y() - top_left.y());
}

gfx::PointF ActionMove::TransformLocationInPixels(
    const gfx::RectF& content_bounds,
    const gfx::PointF& root_location) {
  auto target_area = CalculateApplyArea(content_bounds);
  auto new_pos = gfx::PointF();
  if (target_area) {
    auto orig_point = root_location - content_bounds.origin();
    float ratio = orig_point.x() / content_bounds.width();
    float x = ratio * target_area->width() + target_area->x();
    ratio = orig_point.y() / content_bounds.height();
    float y = ratio * target_area->height() + target_area->y();
    new_pos.SetPoint(x, y);
    DCHECK(target_area->Contains(new_pos));
  } else {
    new_pos.SetPoint(root_location.x(), root_location.y());
  }

  float scale = target_window_->GetHost()->device_scale_factor();
  new_pos.Scale(scale);
  return new_pos;
}

}  // namespace input_overlay
}  // namespace arc
