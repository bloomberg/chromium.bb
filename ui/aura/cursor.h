// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CURSOR_H_
#define UI_AURA_CURSOR_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {

enum AURA_EXPORT CursorType {
  CURSOR_POINTER,
  CURSOR_LINK,
  CURSOR_WAIT,
  CURSOR_SIZE_HORIZONTAL,
  CURSOR_SIZE_VERTICAL
};

}  // namespace aura

#endif  // UI_AURA_CURSOR_H_
