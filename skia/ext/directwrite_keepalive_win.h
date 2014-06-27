// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_DIRECTWRITE_KEEPALIVE_WIN_H_
#define SKIA_EXT_DIRECTWRITE_KEEPALIVE_WIN_H_

#include "third_party/skia/include/core/SkTypes.h"

class SkFontMgr;

// Sends a command to the Font Cache Service to avoid being dropped.
void SK_API SkiaDirectWriteKeepalive(SkFontMgr* fontmgr);

#endif  // SKIA_EXT_DIRECTWRITE_KEEPALIVE_WIN_H_
