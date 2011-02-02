// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_UTILS_GTK_H_
#define UI_GFX_SKIA_UTILS_GTK_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"

typedef struct _GdkColor GdkColor;

namespace gfx {

// Converts GdkColors to the ARGB layout Skia expects.
SkColor GdkColorToSkColor(GdkColor color);

// Converts ARGB to GdkColor.
GdkColor SkColorToGdkColor(SkColor color);

}  // namespace gfx

#endif  // UI_GFX_SKIA_UTILS_GTK_H_
