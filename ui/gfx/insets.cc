// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/insets.h"

#include "base/string_util.h"

namespace gfx {

std::string Insets::ToString() const {
  // Print members in the same order of the constructor parameters.
  return StringPrintf("%d,%d,%d,%d", top_,  left_, bottom_, right_);
}

}  // namespace gfx
