// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_GTK_H_
#define SKIA_EXT_SKIA_UTILS_GTK_H_

#include "third_party/skia/include/core/SkColor.h"

typedef struct _GdkColor GdkColor;

namespace skia {

// Converts GdkColors to the ARGB layout Skia expects.
SkColor GdkColorToSkColor(GdkColor color);

// Converts ARGB to GdkColor.
GdkColor SkColorToGdkColor(SkColor color);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_GTK_H_
