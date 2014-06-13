// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_X11_PROPERTY_CHANGE_WAITER_H_
#define UI_VIEWS_TEST_X11_PROPERTY_CHANGE_WAITER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_types.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {
class ScopedEventDispatcher;
class X11AtomCache;
}

namespace views {

// Blocks till the value of |property| on |window| changes.
class X11PropertyChangeWaiter : public ui::PlatformEventDispatcher {
 public:
  X11PropertyChangeWaiter(XID window, const char* property);
  virtual ~X11PropertyChangeWaiter();

  // Blocks till the value of |property_| changes.
  virtual void Wait();

 protected:
  // Returns whether the run loop can exit.
  virtual bool ShouldKeepOnWaiting(const ui::PlatformEvent& event);

  XID xwindow() const {
    return x_window_;
  }

 private:
  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE;

  XID x_window_;
  const char* property_;

  // Whether Wait() should block.
  bool wait_;

  // Ends the run loop.
  base::Closure quit_closure_;

  // The event mask to be restored upon X11PropertyChangeWaiter's destruction.
  long old_event_mask_;

  scoped_ptr<ui::ScopedEventDispatcher> dispatcher_;

  scoped_ptr<ui::X11AtomCache> atom_cache_;

  DISALLOW_COPY_AND_ASSIGN(X11PropertyChangeWaiter);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_X11_PROPERTY_CHANGE_WAITER_H_
