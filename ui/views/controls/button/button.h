// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/painter.h"

namespace views {

class Button;
class Event;

// An interface implemented by an object to let it know that a button was
// pressed.
class VIEWS_EXPORT ButtonListener {
 public:
  virtual void ButtonPressed(Button* sender, const ui::Event& event) = 0;

 protected:
  virtual ~ButtonListener() {}
};

// A View representing a button. A Button is not focusable by default and will
// not be part of the focus chain, unless in accessibility mode (see
// SetFocusForPlatform()).
class VIEWS_EXPORT Button : public InkDropHostView,
                            public gfx::AnimationDelegate {
 public:
  ~Button() override;

  // Button states for various button sub-types.
  enum ButtonState {
    STATE_NORMAL = 0,
    STATE_HOVERED,
    STATE_PRESSED,
    STATE_DISABLED,
    STATE_COUNT,
  };

  // Button styles with associated images and border painters.
  // TODO(msw): Add Menu, ComboBox, etc.
  enum ButtonStyle {
    STYLE_BUTTON = 0,
    STYLE_TEXTBUTTON,
    STYLE_COUNT,
  };

  // An enum describing the events on which a button should notify its listener.
  enum NotifyAction {
    NOTIFY_ON_PRESS,
    NOTIFY_ON_RELEASE,
  };

  // An enum describing the events on which a button should be clicked for a
  // given key event.
  enum KeyClickAction {
    CLICK_ON_KEY_PRESS,
    CLICK_ON_KEY_RELEASE,
    CLICK_NONE,
  };

  // The menu button's class name.
  static const char kViewClassName[];

  static const Button* AsButton(const View* view);
  static Button* AsButton(View* view);

  static ButtonState GetButtonStateFrom(ui::NativeTheme::State state);

  // Make the button focusable as per the platform.
  void SetFocusForPlatform();

  void SetTooltipText(const base::string16& tooltip_text);

  int tag() const { return tag_; }
  void set_tag(int tag) { tag_ = tag; }

  void SetAccessibleName(const base::string16& name);
  const base::string16& accessible_name() const { return accessible_name_; }

  // Get/sets the current display state of the button.
  ButtonState state() const { return state_; }
  // Clients passing in STATE_DISABLED should consider calling
  // SetEnabled(false) instead because the enabled flag can affect other things
  // like event dispatching, focus traversals, etc. Calling SetEnabled(false)
  // will also set the state of |this| to STATE_DISABLED.
  void SetState(ButtonState state);

  // Starts throbbing. See HoverAnimation for a description of cycles_til_stop.
  // This method does nothing if |animate_on_state_change_| is false.
  void StartThrobbing(int cycles_til_stop);

  // Stops throbbing immediately.
  void StopThrobbing();

  // Set how long the hover animation will last for.
  void SetAnimationDuration(int duration);

  void set_triggerable_event_flags(int triggerable_event_flags) {
    triggerable_event_flags_ = triggerable_event_flags;
  }
  int triggerable_event_flags() const { return triggerable_event_flags_; }

  // Sets whether |RequestFocus| should be invoked on a mouse press. The default
  // is false.
  void set_request_focus_on_press(bool value) {
// On Mac, buttons should not request focus on a mouse press. Hence keep the
// default value i.e. false.
#if !defined(OS_MACOSX)
    request_focus_on_press_ = value;
#endif
  }

  bool request_focus_on_press() const { return request_focus_on_press_; }

  // See description above field.
  void set_animate_on_state_change(bool value) {
    animate_on_state_change_ = value;
  }

  // Sets the event on which the button should notify its listener.
  void set_notify_action(NotifyAction notify_action) {
    notify_action_ = notify_action;
  }

  void set_hide_ink_drop_when_showing_context_menu(
      bool hide_ink_drop_when_showing_context_menu) {
    hide_ink_drop_when_showing_context_menu_ =
        hide_ink_drop_when_showing_context_menu;
  }

  void set_ink_drop_base_color(SkColor color) { ink_drop_base_color_ = color; }
  void set_has_ink_drop_action_on_click(bool has_ink_drop_action_on_click) {
    has_ink_drop_action_on_click_ = has_ink_drop_action_on_click;
  }

  void SetHotTracked(bool is_hot_tracked);
  bool IsHotTracked() const;

  void SetFocusPainter(std::unique_ptr<Painter> focus_painter);

  // Overridden from View:
  void OnEnabledChanged() override;
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  void OnDragDone() override;
  // Instead of overriding this, subclasses that want custom painting should use
  // PaintButtonContents.
  void OnPaint(gfx::Canvas* canvas) final;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnFocus() override;
  void OnBlur() override;

  // Overridden from InkDropHostView:
  std::unique_ptr<InkDrop> CreateInkDrop() override;
  SkColor GetInkDropBaseColor() const override;

  // Overridden from gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 protected:
  // Construct the Button with a Listener. The listener can be null. This can be
  // true of buttons that don't have a listener - e.g. menubuttons where there's
  // no default action and checkboxes.
  explicit Button(ButtonListener* listener);

  // Cause the button to notify the listener that a click occurred.
  virtual void NotifyClick(const ui::Event& event);

  // Called when a button gets released without triggering an action.
  // Note: This is only wired up for mouse button events and not gesture
  // events.
  virtual void OnClickCanceled(const ui::Event& event);

  // Invoked from SetState() when SetState() is passed a value that differs from
  // the current node_data. Button's implementation of StateChanged() does
  // nothing; this method is provided for subclasses that wish to do something
  // on state changes.
  virtual void StateChanged(ButtonState old_state);

  // Returns true if the event is one that can trigger notifying the listener.
  // This implementation returns true if the left mouse button is down.
  virtual bool IsTriggerableEvent(const ui::Event& event);

  // Returns true if the ink drop should be updated by Button when
  // OnClickCanceled() is called. This method is provided for subclasses.
  // If the method is overriden and returns false, the subclass is responsible
  // will be responsible for updating the ink drop.
  virtual bool ShouldUpdateInkDropOnClickCanceled() const;

  // Returns true if the button should become pressed when the user
  // holds the mouse down over the button. For this implementation,
  // we simply return IsTriggerableEvent(event).
  virtual bool ShouldEnterPushedState(const ui::Event& event);

  // Override to paint custom button contents. Any background or border set on
  // the view will be painted before this is called and |focus_painter_| will be
  // painted afterwards.
  virtual void PaintButtonContents(gfx::Canvas* canvas);

  // Returns true if the button should enter hovered state; that is, if the
  // mouse is over the button, and no other window has capture (which would
  // prevent the button from receiving MouseExited events and updating its
  // node_data). This does not take into account enabled node_data.
  bool ShouldEnterHoveredState();

  const gfx::ThrobAnimation& hover_animation() const {
    return hover_animation_;
  }

  // The button's listener. Notified when clicked.
  ButtonListener* listener_;

 private:
  FRIEND_TEST_ALL_PREFIXES(BlueButtonTest, Border);

  // The text shown in a tooltip.
  base::string16 tooltip_text_;

  // Accessibility data.
  base::string16 accessible_name_;

  // The id tag associated with this button. Used to disambiguate buttons in
  // the ButtonListener implementation.
  int tag_ = -1;

  ButtonState state_ = STATE_NORMAL;

  gfx::ThrobAnimation hover_animation_{this};

  // Should we animate when the state changes?
  bool animate_on_state_change_ = false;

  // Is the hover animation running because StartThrob was invoked?
  bool is_throbbing_ = false;

  // Mouse event flags which can trigger button actions.
  int triggerable_event_flags_ = ui::EF_LEFT_MOUSE_BUTTON;

  // See description above setter.
  bool request_focus_on_press_ = false;

  // The event on which the button should notify its listener.
  NotifyAction notify_action_ = NOTIFY_ON_RELEASE;

  // True when a button click should trigger an animation action on
  // ink_drop_delegate().
  bool has_ink_drop_action_on_click_ = false;

  // When true, the ink drop ripple and hover will be hidden prior to showing
  // the context menu.
  bool hide_ink_drop_when_showing_context_menu_ = true;

  // The color of the ripple and hover.
  SkColor ink_drop_base_color_;

  std::unique_ptr<Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(Button);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_
