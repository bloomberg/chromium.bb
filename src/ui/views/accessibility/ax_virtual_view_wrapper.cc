// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view_wrapper.h"

#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/accessibility/ax_virtual_view.h"

namespace views {

AXVirtualViewWrapper::AXVirtualViewWrapper(AXVirtualView* virtual_view)
    : virtual_view_(virtual_view) {}

AXVirtualViewWrapper::~AXVirtualViewWrapper() = default;

bool AXVirtualViewWrapper::IsIgnored() {
  return false;
}

AXAuraObjWrapper* AXVirtualViewWrapper::GetParent() {
  if (virtual_view_->virtual_parent_view())
    return virtual_view_->virtual_parent_view()->GetWrapper();
  if (virtual_view_->GetOwnerView())
    return AXAuraObjCache::GetInstance()->GetOrCreate(
        virtual_view_->GetOwnerView());

  return nullptr;
}

void AXVirtualViewWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
  for (int i = 0; i < virtual_view_->GetChildCount(); ++i)
    out_children->push_back(virtual_view_->child_at(i)->GetWrapper());
}

void AXVirtualViewWrapper::Serialize(ui::AXNodeData* out_node_data) {
  *out_node_data = virtual_view_->GetData();
}

int32_t AXVirtualViewWrapper::GetUniqueId() const {
  return virtual_view_->GetUniqueId().Get();
}

bool AXVirtualViewWrapper::HandleAccessibleAction(
    const ui::AXActionData& action) {
  return virtual_view_->HandleAccessibleAction(action);
}

}  // namespace views
