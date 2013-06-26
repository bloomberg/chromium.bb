// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PATH_X11_H_
#define UI_GFX_PATH_X11_H_

#include <X11/Xlib.h>
#include <X11/Xregion.h>

#include "ui/base/ui_export.h"

class SkPath;

namespace gfx {

// Creates a new REGION given |path|. The caller is responsible for destroying
// the returned region.
UI_EXPORT REGION* CreateRegionFromSkPath(const SkPath& path);

}  // namespace gfx

#endif  // UI_GFX_PATH_X11_H_
