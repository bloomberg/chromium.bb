// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vector2d_f.h"
#include "ui/views/widget/desktop_aura/x11_move_loop.h"
#include "ui/views/widget/desktop_aura/x11_move_loop_delegate.h"

typedef struct _XDisplay XDisplay;

namespace aura {
class Window;
}

namespace ui {
class ScopedEventDispatcher;
}

namespace views {

class Widget;

// Runs a nested message loop and grabs the mouse. This is used to implement
// dragging.
class X11WholeScreenMoveLoop : public X11MoveLoop,
                               public ui::PlatformEventDispatcher {
 public:
  explicit X11WholeScreenMoveLoop(X11MoveLoopDelegate* delegate);
  virtual ~X11WholeScreenMoveLoop();

  // ui:::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE;

  // X11MoveLoop:
  virtual bool RunMoveLoop(aura::Window* window,
                           gfx::NativeCursor cursor) OVERRIDE;
  virtual void UpdateCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void EndMoveLoop() OVERRIDE;

 private:
  // Grabs the pointer and keyboard, setting the mouse cursor to |cursor|.
  // Returns true if the grab was successful.
  bool GrabPointerAndKeyboard(gfx::NativeCursor cursor);

  // Creates an input-only window to be used during the drag.
  Window CreateDragInputWindow(XDisplay* display);

  // Dispatch mouse movement event to |delegate_| in a posted task.
  void DispatchMouseMovement();

  X11MoveLoopDelegate* delegate_;

  // Are we running a nested message loop from RunMoveLoop()?
  bool in_move_loop_;
  scoped_ptr<ui::ScopedEventDispatcher> nested_dispatcher_;

  bool should_reset_mouse_flags_;

  // An invisible InputOnly window . We create this window so we can track the
  // cursor wherever it goes on screen during a drag, since normal windows
  // don't receive pointer motion events outside of their bounds.
  ::Window grab_input_window_;

  base::Closure quit_closure_;

  // Keeps track of whether the move-loop is cancled by the user (e.g. by
  // pressing escape).
  bool canceled_;

  // Keeps track of whether we still have a pointer grab at the end of the loop.
  bool has_grab_;

  XMotionEvent last_xmotion_;
  base::WeakPtrFactory<X11WholeScreenMoveLoop> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(X11WholeScreenMoveLoop);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_
