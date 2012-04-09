// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/insets_f.h"

#include "base/stringprintf.h"

namespace gfx {

std::string InsetsF::ToString() const {
  // Print members in the same order of the constructor parameters.
  return StringPrintf("%f,%f,%f,%f", top_,  left_, bottom_, right_);
}

}  // namespace gfx
