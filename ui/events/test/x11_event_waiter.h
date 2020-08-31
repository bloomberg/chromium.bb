// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_X11_EVENT_WAITER_H_
#define UI_EVENTS_TEST_X11_EVENT_WAITER_H_

#include <memory>

#include "base/callback.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

class ScopedXEventDispatcher;

// X11 Event Waiter class
class XEventWaiter : public ui::XEventObserver {
 public:
  static XEventWaiter* Create(XID window, base::OnceClosure callback);

 private:
  explicit XEventWaiter(base::OnceClosure callback);
  ~XEventWaiter() override;

  // ui::XEventObserver:
  void DidProcessXEvent(XEvent* xev) override {}
  void WillProcessXEvent(XEvent* xev) override;

  // Returns atom that indidates that the XEvent is marker event.
  static XAtom MarkerEventAtom();

  base::OnceClosure success_callback_;
  std::unique_ptr<ui::ScopedXEventDispatcher> dispatcher_;
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_X11_EVENT_WAITER_H_
