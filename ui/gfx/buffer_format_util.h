// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_FORMAT_UTIL_H_
#define UI_GFX_BUFFER_FORMAT_UTIL_H_

#include "base/basictypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Returns the number of planes for |format|.
GFX_EXPORT size_t NumberOfPlanesForBufferFormat(BufferFormat format);

}  // namespace gfx

#endif  // UI_GFX_BUFFER_FORMAT_UTIL_H_
