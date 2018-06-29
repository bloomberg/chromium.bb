// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/ax_tree_source_mus.h"

#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace views {

AXTreeSourceMus::AXTreeSourceMus(AXAuraObjWrapper* root) : root_(root) {
  DCHECK(root_);
}

AXTreeSourceMus::~AXTreeSourceMus() = default;

AXAuraObjWrapper* AXTreeSourceMus::GetRoot() const {
  return root_;
}

void AXTreeSourceMus::SerializeNode(AXAuraObjWrapper* node,
                                    ui::AXNodeData* out_data) const {
  if (IsEqual(node, root_)) {
    node->Serialize(out_data);
    // Root is a ClientView with an offset from the containing Widget. However,
    // the ClientView in the host (browser) already has an offset from its
    // Widget, so the root should start at (0,0).
    out_data->location.set_origin(gfx::PointF());
    out_data->transform.reset();
    return;
  }

  AXTreeSourceViews::SerializeNode(node, out_data);
}

}  // namespace views
