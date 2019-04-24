/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkTestFontMgr_DEFINED
#define SkTestFontMgr_DEFINED

#include "SkFontMgr.h"

// An SkFontMgr that always uses sk_tool_utils::create_portable_typeface().

namespace sk_tool_utils {
    sk_sp<SkFontMgr> MakePortableFontMgr();
}  // namespace sk_tool_utils

#endif  //SkTestFontMgr_DEFINED
