// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_id.h"

#include "base/no_destructor.h"

namespace ui {

const AXTreeID& AXTreeIDUnknown() {
  static const base::NoDestructor<AXTreeID> ax_tree_id_unknown("");
  return *ax_tree_id_unknown;
}

const AXTreeID& DesktopAXTreeID() {
  static const base::NoDestructor<AXTreeID> desktop_ax_tree_id("0");
  return *desktop_ax_tree_id;
}

}  // namespace ui
