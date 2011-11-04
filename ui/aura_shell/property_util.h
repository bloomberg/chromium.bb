// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_PROPERTY_UTIL_H_
#define UI_AURA_SHELL_PROPERTY_UTIL_H_
#pragma once

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace aura_shell {

// Sets the restore bounds property on |window|. Deletes existing bounds value
// if exists.
void SetRestoreBounds(aura::Window* window, const gfx::Rect& bounds);

// Same as SetRestoreBounds(), but does nothing if the restore bounds have
// already been set. The bounds used are the bounds of the window.
void SetRestoreBoundsIfNotSet(aura::Window* window);

// Returns the restore bounds property on |window|. NULL if the
// restore bounds property does not exist for |window|. |window|
// owns the bounds object.
const gfx::Rect* GetRestoreBounds(aura::Window* window);

// Deletes and clears the restore bounds property on |window|.
void ClearRestoreBounds(aura::Window* window);

}

#endif  // UI_AURA_SHELL_PROPERTY_UTIL_H_
