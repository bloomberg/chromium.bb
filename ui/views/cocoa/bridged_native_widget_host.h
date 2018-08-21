// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_H_

#include "ui/events/event_utils.h"
#include "ui/gfx/decorated_text.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/views_export.h"

namespace views {

// The interface through which the app shim (BridgedNativeWidgetImpl)
// communicates with the browser process (BridgedNativeWidgetHostImpl).
class VIEWS_EXPORT BridgedNativeWidgetHost {
 public:
  virtual ~BridgedNativeWidgetHost() = default;

  // Update the views::Widget, ui::Compositor and ui::Layer's visibility.
  virtual void OnVisibilityChanged(bool visible) = 0;

  // Resize the underlying views::View to |new_size|. Note that this will not
  // necessarily match the content bounds from OnWindowGeometryChanged.
  virtual void SetViewSize(const gfx::Size& new_size) = 0;

  // Indicate if full keyboard accessibility is needed and updates focus if
  // needed.
  virtual void SetKeyboardAccessible(bool enabled) = 0;

  // Indicate if the NSView is the first responder.
  virtual void SetIsFirstResponder(bool is_first_responder) = 0;

  // Handle events. Note that whether or not the event is actually handled is
  // not returned.
  virtual void OnScrollEvent(const ui::ScrollEvent& const_event) = 0;
  virtual void OnMouseEvent(const ui::MouseEvent& const_event) = 0;
  virtual void OnGestureEvent(const ui::GestureEvent& const_event) = 0;

  // Synchronously query if |location_in_content| is a draggable background.
  virtual void GetIsDraggableBackgroundAt(const gfx::Point& location_in_content,
                                          bool* is_draggable_background) = 0;

  // Synchronously query the tooltip text for |location_in_content|.
  virtual void GetTooltipTextAt(const gfx::Point& location_in_content,
                                base::string16* new_tooltip_text) = 0;

  // Synchronously query the quicklook text at |location_in_content|. Return in
  // |found_word| whether or not a word was found.
  virtual void GetWordAt(const gfx::Point& location_in_content,
                         bool* found_word,
                         gfx::DecoratedText* decorated_word,
                         gfx::Point* baseline_point) = 0;

  // Called whenever the NSWindow's size or position changes.
  virtual void OnWindowGeometryChanged(
      const gfx::Rect& window_bounds_in_screen_dips,
      const gfx::Rect& content_bounds_in_screen_dips) = 0;

  // Called when the window begins transitioning to or from being fullscreen.
  virtual void OnWindowFullscreenTransitionStart(
      bool target_fullscreen_state) = 0;

  // Called when the window has completed its transition to or from being
  // fullscreen. Note that if there are multiple consecutive transitions
  // (because a new transition was initiated before the previous one completed)
  // then this will only be called when all transitions have competed.
  virtual void OnWindowFullscreenTransitionComplete(bool is_fullscreen) = 0;

  // Called when the window is miniaturized or deminiaturized.
  virtual void OnWindowMiniaturizedChanged(bool miniaturized) = 0;

  // Called when the current display or the properties of the current display
  // change.
  virtual void OnWindowDisplayChanged(const display::Display& display) = 0;

  // Called before the NSWindow is closed and destroyed.
  virtual void OnWindowWillClose() = 0;

  // Called after the NSWindow has been closed and destroyed.
  virtual void OnWindowHasClosed() = 0;

  // Called when the NSWindow becomes key or resigns from being key. Additional
  // state required for the transition include whether or not the content NSView
  // is the first responder for the NSWindow in |is_content_first_responder| and
  // whether or not the NSApp's full keyboard access is enabled in
  // |full_keyboard_access_enabled|.
  virtual void OnWindowKeyStatusChanged(bool is_key,
                                        bool is_content_first_responder,
                                        bool full_keyboard_access_enabled) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_H_
