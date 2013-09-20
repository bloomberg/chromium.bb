// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_UTIL_H_
#define UI_GFX_X_X11_UTIL_H_

#include "ui/gfx/gfx_export.h"

typedef unsigned long XID;
typedef struct _XImage XImage;
typedef struct _XGC *GC;
typedef struct _XDisplay XDisplay;

namespace gfx {

// TODO(oshima|evan): This assume there is one display and doesn't work
// undef multiple displays/monitor environment. Remove this and change the
// chrome codebase to get the display from window.
GFX_EXPORT XDisplay* GetXDisplay();

}  // namespace gfx

#endif  // UI_GFX_X_X11_UTIL_H_

