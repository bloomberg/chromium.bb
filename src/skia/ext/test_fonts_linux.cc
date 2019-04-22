// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts.h"

#include "base/test/fontconfig_util_linux.h"

namespace skia {

void ConfigureTestFont() {
  base::SetUpFontconfig();
}

}  // namespace skia
