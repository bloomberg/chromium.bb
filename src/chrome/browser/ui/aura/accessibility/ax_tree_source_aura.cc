// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/ax_tree_source_aura.h"

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

AXTreeSourceAura::AXTreeSourceAura()
    : desktop_root_(std::make_unique<AXRootObjWrapper>(
          AutomationManagerAura::GetInstance())) {
  Init(desktop_root_.get(), ui::DesktopAXTreeID());
}

AXTreeSourceAura::~AXTreeSourceAura() = default;

void AXTreeSourceAura::SerializeNode(views::AXAuraObjWrapper* node,
                                     ui::AXNodeData* out_data) const {
  AXTreeSourceViews::SerializeNode(node, out_data);
  if (out_data->role == ax::mojom::Role::kWindow ||
      out_data->role == ax::mojom::Role::kDialog) {
    // Add clips children flag by default to these roles.
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);
  }
}
