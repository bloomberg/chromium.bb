// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/ax_tree_source_mus.h"

#include <vector>

#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace views {

AXTreeSourceMus::AXTreeSourceMus(AXAuraObjWrapper* root) : root_(root) {
  DCHECK(root_);
}

AXTreeSourceMus::~AXTreeSourceMus() = default;

bool AXTreeSourceMus::GetTreeData(ui::AXTreeData* tree_data) const {
  tree_data->tree_id = 0;
  tree_data->loaded = true;
  tree_data->loading_progress = 1.0;
  // TODO(jamescook): Support focus in more than one app.
  AXAuraObjWrapper* focus = AXAuraObjCache::GetInstance()->GetFocus();
  if (focus)
    tree_data->focus_id = focus->GetUniqueId().Get();
  return true;
}

AXAuraObjWrapper* AXTreeSourceMus::GetRoot() const {
  return root_;
}

AXAuraObjWrapper* AXTreeSourceMus::GetFromId(int32_t id) const {
  return AXAuraObjCache::GetInstance()->Get(id);
}

int32_t AXTreeSourceMus::GetId(AXAuraObjWrapper* node) const {
  return node->GetUniqueId().Get();
}

void AXTreeSourceMus::GetChildren(
    AXAuraObjWrapper* node,
    std::vector<AXAuraObjWrapper*>* out_children) const {
  node->GetChildren(out_children);
}

AXAuraObjWrapper* AXTreeSourceMus::GetParent(AXAuraObjWrapper* node) const {
  // Stop at |root_| even if it has a views parent.
  if (node->GetUniqueId() == root_->GetUniqueId())
    return nullptr;
  return node->GetParent();
}

bool AXTreeSourceMus::IsValid(AXAuraObjWrapper* node) const {
  return node != nullptr;
}

bool AXTreeSourceMus::IsEqual(AXAuraObjWrapper* node1,
                              AXAuraObjWrapper* node2) const {
  return node1 && node2 && node1->GetUniqueId() == node2->GetUniqueId();
}

AXAuraObjWrapper* AXTreeSourceMus::GetNull() const {
  return nullptr;
}

void AXTreeSourceMus::SerializeNode(AXAuraObjWrapper* node,
                                    ui::AXNodeData* out_data) const {
  node->Serialize(out_data);

  if (IsEqual(node, root_)) {
    // Root is a ClientView with an offset from the containing Widget. However,
    // the ClientView in the host (browser) already has an offset from its
    // Widget, so the root should start at (0,0).
    out_data->location.set_origin(gfx::PointF());
    out_data->transform.reset();
    return;
  }

  // Convert the global coordinates reported by each AXAuraObjWrapper
  // into parent-relative coordinates to be used in the accessibility
  // tree. That way when any Window, Widget, or View moves (and fires
  // a location changed event), its descendants all move relative to
  // it by default.
  AXAuraObjWrapper* parent = node->GetParent();
  DCHECK(parent);
  ui::AXNodeData parent_data;
  parent->Serialize(&parent_data);
  out_data->location.Offset(-parent_data.location.OffsetFromOrigin());
  out_data->offset_container_id = parent->GetUniqueId().Get();
}

}  // namespace views
