// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OOBE_OOBE_EXPORT_H_
#define UI_OOBE_OOBE_EXPORT_H_

// Defines OOBE_UI_EXPORT so that functionality implemented by the
// keyboard module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(OOBE_IMPLEMENTATION)
#define OOBE_EXPORT __declspec(dllexport)
#else
#define OOBE_EXPORT __declspec(dllimport)
#endif  // defined(OOBE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(OOBE_IMPLEMENTATION)
#define OOBE_EXPORT __attribute__((visibility("default")))
#else
#define OOBE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define OOBE_EXPORT
#endif

#endif  // UI_OOBE_OOBE_EXPORT_H_
