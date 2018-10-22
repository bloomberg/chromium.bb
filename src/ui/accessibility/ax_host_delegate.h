// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_HOST_DELEGATE_H_
#define UI_ACCESSIBILITY_AX_HOST_DELEGATE_H_

#include <stdint.h>

#include "ui/accessibility/ax_export.h"

namespace ui {

struct AXActionData;

// Classes that host an accessibility tree in the browser process that also wish
// to become visible to accessibility clients (e.g. for relaying targets to
// source accessibility trees), can subclass this delegate.
//
// Subclasses can use |tree_id| when annotating their |AXNodeData| for clients
// to respond with the appropriate target node id.
class AX_EXPORT AXHostDelegate {
 public:
  virtual ~AXHostDelegate();

  // Handle an action from an accessibility client.
  virtual void PerformAction(const ui::AXActionData& data) = 0;

 protected:
  // A delegate with an automatically assigned tree id.
  AXHostDelegate();

  // A delegate with an explicit tree id. The caller is responsible for ensuring
  // the uniqueness of the id.
  explicit AXHostDelegate(int32_t tree_id);

  // A tree id appropriate for annotating events sent to an accessibility
  // client.
  int32_t tree_id() const { return tree_id_; }

 private:
  // Register or unregister this class with |AXTreeIDRegistry|.
  void UpdateActiveState(bool active);

  int32_t tree_id_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_HOST_DELEGATE_H_
