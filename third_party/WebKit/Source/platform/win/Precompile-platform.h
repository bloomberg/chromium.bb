// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(PrecompilePlatform_h_)
#error You shouldn't include the precompiled header file more than once.
#endif

#define PrecompilePlatform_h_

#include "build/win/Precompile.h"

// Include Oilpan's Handle.h by default, as it is included by a significant
// portion of platform/ source files.
#include "platform/heap/Handle.h"
