// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_WINDOW_EVENT_MANAGER_H_
#define UI_BASE_X_X11_WINDOW_EVENT_MANAGER_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/x/ui_base_x_export.h"
#include "ui/gfx/x/x11_types.h"

// A process wide singleton for selecting events on X windows which were not
// created by Chrome.
namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// Ensures events in |event_mask| are selected on |xid| for the duration of this
// object's lifetime.
class UI_BASE_X_EXPORT XScopedEventSelector {
 public:
  XScopedEventSelector(XID xid, uint32_t event_mask);
  ~XScopedEventSelector();

 private:
  XID xid_;
  uint32_t event_mask_;

  DISALLOW_COPY_AND_ASSIGN(XScopedEventSelector);
};

// Manages the events that Chrome has selected on X windows which were not
// created by Chrome. This class allows multiple clients within Chrome to select
// events on the same X window.
class UI_BASE_X_EXPORT XWindowEventManager {
 public:
  static XWindowEventManager* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<XWindowEventManager>;
  friend class XScopedEventSelector;

  class MultiMask;

  XWindowEventManager();
  ~XWindowEventManager();

  // Guarantees that events in |event_mask| will be reported to Chrome.
  void SelectEvents(XID xid, uint32_t event_mask);

  // Deselects events on |event_mask|.  Chrome will stop receiving events for
  // any set bit in |event_mask| only if no other client has selected that bit.
  void DeselectEvents(XID xid, uint32_t event_mask);

  // Called whenever the mask corresponding to window |xid| might have changed.
  // Updates the event mask with respect to the server, if necessary.
  void AfterMaskChanged(XID xid, uint32_t old_mask);

  std::map<XID, std::unique_ptr<MultiMask>> mask_map_;

  DISALLOW_COPY_AND_ASSIGN(XWindowEventManager);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_FOREIGN_WINDOW_MANAGER_H_
