// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/util/win8_util.h"

#include "base/win/metro.h"

namespace win8 {

bool IsSingleWindowMetroMode() {
#if defined(USE_ASH)
  return false;
#else
  return base::win::IsMetroProcess();
#endif
}

}  // namespace win8
