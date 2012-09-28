// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SAFE_FLOOR_CEIL_H_
#define UI_GFX_SAFE_FLOOR_CEIL_H_

namespace gfx {

UI_EXPORT int ClampToInt(float value);
UI_EXPORT int ToFlooredInt(float value);
UI_EXPORT int ToCeiledInt(float value);

}  // namespace gfx

#endif  // UI_GFX_SAFE_FLOOR_CEIL_H_
