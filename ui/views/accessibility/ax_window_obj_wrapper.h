// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_

#include "ui/aura/window_observer.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {

// Describes a |Window| for use with other AX classes.
class AXWindowObjWrapper : public AXAuraObjWrapper,
                           public aura::WindowObserver {
 public:
  explicit AXWindowObjWrapper(aura::Window* window);
  ~AXWindowObjWrapper() override;

  // AXAuraObjWrapper overrides.
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32 GetID() override;

  // WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) override;

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(AXWindowObjWrapper);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
