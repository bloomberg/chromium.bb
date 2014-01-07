// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_WINDOW_UTIL_H_
#define UI_VIEWS_COREWM_WINDOW_UTIL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
}

namespace views {
namespace corewm {

VIEWS_EXPORT void ActivateWindow(aura::Window* window);
VIEWS_EXPORT void DeactivateWindow(aura::Window* window);
VIEWS_EXPORT bool IsActiveWindow(aura::Window* window);
VIEWS_EXPORT bool CanActivateWindow(aura::Window* window);

// Retrieves the activatable window for |window|. The ActivationClient makes
// this determination.
VIEWS_EXPORT aura::Window* GetActivatableWindow(aura::Window* window);

// Retrieves the toplevel window for |window|. The ActivationClient makes this
// determination.
VIEWS_EXPORT aura::Window* GetToplevelWindow(aura::Window* window);

// Deletes |layer| and all its child layers.
VIEWS_EXPORT void DeepDeleteLayers(ui::Layer* layer);

// Returns the existing Layer for |window| (and all its descendants) and creates
// a new layer for |window| and all its descendants. This is intended for
// animations that want to animate between the existing visuals and a new window
// state. The caller owns the return value.
//
// As a result of this |window| has freshly created layers, meaning the layers
// are all empty (nothing has been painted to them) and are sized to 0x0. Soon
// after this call you need to reset the bounds of the window. Or, you can pass
// true as the second argument to let the function do that.
VIEWS_EXPORT ui::Layer* RecreateWindowLayers(aura::Window* window,
                                            bool set_bounds) WARN_UNUSED_RESULT;

// Convenience functions that get the TransientWindowManager for the window and
// redirect appropriately. These are preferable to calling functions on
// TransientWindowManager as they handle the appropriate NULL checks.
VIEWS_EXPORT aura::Window* GetTransientParent(aura::Window* window);
VIEWS_EXPORT const aura::Window* GetTransientParent(const aura::Window* window);
VIEWS_EXPORT const std::vector<aura::Window*>& GetTransientChildren(
    const aura::Window* window);
VIEWS_EXPORT void AddTransientChild(aura::Window* parent, aura::Window* child);
VIEWS_EXPORT void RemoveTransientChild(aura::Window* parent,
                                       aura::Window* child);

// Returns true if |window| has |ancestor| as a transient ancestor. A transient
// ancestor is found by following the transient parent chain of the window.
VIEWS_EXPORT bool HasTransientAncestor(const aura::Window* window,
                                       const aura::Window* ancestor);

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_WINDOW_UTIL_H_
