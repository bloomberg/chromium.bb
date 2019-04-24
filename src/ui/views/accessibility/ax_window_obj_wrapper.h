// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/window_observer.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class AXAuraObjCache;

// Describes a |Window| for use with other AX classes.
class AXWindowObjWrapper : public AXAuraObjWrapper,
                           public aura::WindowObserver {
 public:
  // |aura_obj_cache| and |window| must outlive this object.
  AXWindowObjWrapper(AXAuraObjCache* aura_obj_cache, aura::Window* window);
  ~AXWindowObjWrapper() override;

  // AXAuraObjWrapper overrides.
  bool IsIgnored() override;
  AXAuraObjWrapper* GetParent() override;
  void GetChildren(std::vector<AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  int32_t GetUniqueId() const final;

  // WindowObserver overrides.
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowTransformed(aura::Window* window,
                           ui::PropertyChangeReason reason) override;
  void OnWindowTitleChanged(aura::Window* window) override;

 private:
  // Fires an event on a window, taking into account its associated widget and
  // that widget's root view.
  void FireEvent(aura::Window* window, ax::mojom::Event event_type);

  AXAuraObjCache* const aura_obj_cache_;

  aura::Window* window_;

  bool is_root_window_;

  const ui::AXUniqueId unique_id_;

  DISALLOW_COPY_AND_ASSIGN(AXWindowObjWrapper);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_WINDOW_OBJ_WRAPPER_H_
