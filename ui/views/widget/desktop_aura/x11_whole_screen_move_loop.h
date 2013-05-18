// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop_delegate.h"

namespace aura {
class Window;
}

namespace views {

// Runs a nested message loop and grabs the mouse. This is used to implement
// dragging.
class X11WholeScreenMoveLoop : public base::MessageLoop::Dispatcher {
 public:
  explicit X11WholeScreenMoveLoop(X11WholeScreenMoveLoopDelegate* delegate);
  virtual ~X11WholeScreenMoveLoop();

  // Overridden from base::MessageLoop::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // Runs the nested message loop. Returns true if there we were able to grab
  // the pointer and run the move loop.
  bool RunMoveLoop(aura::Window* window);

  // Ends the RunMoveLoop() that's currently in progress.
  void EndMoveLoop();

 private:
  X11WholeScreenMoveLoopDelegate* delegate_;

  // Are we running a nested message loop from RunMoveLoop()?
  bool in_move_loop_;

  // An invisible InputOnly window . We create this window so we can track the
  // cursor wherever it goes on screen during a drag, since normal windows
  // don't receive pointer motion events outside of their bounds.
  ::Window grab_input_window_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(X11WholeScreenMoveLoop);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_
