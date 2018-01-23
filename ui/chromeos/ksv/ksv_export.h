// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_KSV_KSV_EXPORT_H_
#define UI_CHROMEOS_KSV_KSV_EXPORT_H_

// Defines KSV_EXPORT so that functionality implemented by
// the keyboard shortcut viewer module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(KSV_IMPLEMENTATION)
#define KSV_EXPORT __declspec(dllexport)
#else
#define KSV_EXPORT __declspec(dllimport)
#endif  // defined(KSV_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(KSV_IMPLEMENTATION)
#define KSV_EXPORT __attribute__((visibility("default")))
#else
#define KSV_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define KSV_EXPORT
#endif

#endif  // UI_CHROMEOS_KSV_KSV_EXPORT_H_
