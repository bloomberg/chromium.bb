// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_STORAGE_EXPORT_H_
#define WEBKIT_COMMON_STORAGE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(STORAGE_IMPLEMENTATION)
#define STORAGE_EXPORT __declspec(dllexport)
#define STORAGE_EXPORT_PRIVATE __declspec(dllexport)
#else
#define STORAGE_EXPORT __declspec(dllimport)
#define STORAGE_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(STORAGE_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(STORAGE_IMPLEMENTATION)
#define STORAGE_EXPORT __attribute__((visibility("default")))
#define STORAGE_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define STORAGE_EXPORT
#define STORAGE_EXPORT_PRIVATE
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define STORAGE_EXPORT
#define STORAGE_EXPORT_PRIVATE
#endif

#endif  // WEBKIT_COMMON_STORAGE_EXPORT_H_
