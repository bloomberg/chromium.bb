// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_PLATFORM_DEVICE_H_
#define SKIA_EXT_VECTOR_PLATFORM_DEVICE_H_

// This file provides an easy way to include the appropriate
// VectorPlatformDevice header file for your platform.
#if defined(WIN32)
#include "skia/ext/vector_platform_device_win.h"
#elif defined(__linux__)
#include "skia/ext/vector_platform_device_linux.h"
#endif

#endif

