// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SHOWER_APP_LIST_SHOWER_EXPORT_H_
#define UI_APP_LIST_SHOWER_APP_LIST_SHOWER_EXPORT_H_

// Defines APP_LIST_SHOWER_EXPORT so that functionality implemented by the
// app_list shower module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(APP_LIST_SHOWER_IMPLEMENTATION)
#define APP_LIST_SHOWER_EXPORT __declspec(dllexport)
#else
#define APP_LIST_SHOWER_EXPORT __declspec(dllimport)
#endif  // defined(APP_LIST_SHOWER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(APP_LIST_SHOWER_IMPLEMENTATION)
#define APP_LIST_SHOWER_EXPORT __attribute__((visibility("default")))
#else
#define APP_LIST_SHOWER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define APP_LIST_SHOWER_EXPORT
#endif

#endif  // UI_APP_LIST_SHOWER_APP_LIST_SHOWER_EXPORT_H_
