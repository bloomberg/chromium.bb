// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view_observer.h"

namespace views {
class View;

// Describes a |View| for use with other AX classes.
class AXViewObjWrapper : public AXAuraObjWrapper, public ViewObserver {
 public:
  explicit AXViewObjWrapper(View* view);
  ~AXViewObjWrapper() override;

  View* view() { return view_; }

  // AXAuraObjWrapper overrides.
  bool IsIgnored() override;
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32_t GetUniqueId() const final;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;

  // ViewObserver overrides.
  void OnViewIsDeleting(View* observed_view) override;

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(AXViewObjWrapper);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_
