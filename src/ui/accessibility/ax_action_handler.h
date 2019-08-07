// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ACTION_HANDLER_H_
#define UI_ACCESSIBILITY_AX_ACTION_HANDLER_H_

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

struct AXActionData;

// Classes that host an accessibility tree in the browser process that also wish
// to become visible to accessibility clients (e.g. for relaying targets to
// source accessibility trees), can subclass this class.
//
// Subclasses can use |tree_id| when annotating their |AXNodeData| for clients
// to respond with the appropriate target node id.
class AX_EXPORT AXActionHandler {
 public:
  virtual ~AXActionHandler();

  // Handle an action from an accessibility client.
  virtual void PerformAction(const AXActionData& data) = 0;

  // Returns whether this handler expects points in pixels (true) or dips
  // (false) for data passed to |PerformAction|.
  virtual bool RequiresPerformActionPointInPixels() const;

  // A tree id appropriate for annotating events sent to an accessibility
  // client.
  const AXTreeID& ax_tree_id() const { return tree_id_; }

 protected:
  AXActionHandler();

 private:
  // Register or unregister this class with |AXTreeIDRegistry|.
  void UpdateActiveState(bool active);

  // Automatically assigned.
  AXTreeID tree_id_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ACTION_HANDLER_H_
