// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_OZONE_EXPORT_H_
#define UI_OZONE_OZONE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(OZONE_IMPLEMENTATION)
#define OZONE_EXPORT __declspec(dllexport)
#else
#define OZONE_EXPORT __declspec(dllimport)
#endif  // defined(OZONE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(OZONE_IMPLEMENTATION)
#define OZONE_EXPORT __attribute__((visibility("default")))
#else
#define OZONE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define OZONE_EXPORT
#endif

#endif  // UI_OZONE_OZONE_EXPORT_H_
