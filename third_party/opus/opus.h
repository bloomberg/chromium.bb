// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a shim header to include the right opus header.
// Use this instead of referencing the opus header directly.

#if defined(USE_SYSTEM_OPUS)
#include <opus/opus.h>
#else
#include "third_party/opus/src/include/opus.h"
#endif
