// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_GPU_OZONE_GPU_EXPORT_H_
#define UI_OZONE_GPU_OZONE_GPU_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(OZONE_GPU_IMPLEMENTATION)
#define OZONE_GPU_EXPORT __declspec(dllexport)
#else
#define OZONE_GPU_EXPORT __declspec(dllimport)
#endif  // defined(OZONE_GPU_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(OZONE_GPU_IMPLEMENTATION)
#define OZONE_GPU_EXPORT __attribute__((visibility("default")))
#else
#define OZONE_GPU_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define OZONE_GPU_EXPORT
#endif

#endif  // UI_OZONE_GPU_OZONE_GPU_EXPORT_H_
