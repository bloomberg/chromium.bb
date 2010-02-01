// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_INTERCEPTORS_H_
#define SANDBOX_SRC_INTERCEPTORS_H_

#if defined(_WIN64)
#include "sandbox/src/interceptors_64.h"
#endif

namespace sandbox {

enum InterceptorId {
  MAP_VIEW_OF_SECTION_ID = 0,
  UNMAP_VIEW_OF_SECTION_ID,
  SET_INFORMATION_THREAD_ID,
  OPEN_THREAD_TOKEN_ID,
  OPEN_THREAD_TOKEN_EX_ID,
  MAX_ID
};

typedef void* OriginalFunctions[MAX_ID];

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTORS_H_
