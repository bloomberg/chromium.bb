// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNC_EXPORT_H_
#define SYNC_SYNC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SYNC_IMPLEMENTATION)
#define SYNC_EXPORT __declspec(dllexport)
#define SYNC_EXPORT_PRIVATE __declspec(dllexport)
#elif defined(SYNC_TEST)
#define SYNC_EXPORT __declspec(dllimport)
#define SYNC_EXPORT_PRIVATE __declspec(dllimport)
#else
#define SYNC_EXPORT __declspec(dllimport)
#define SYNC_EXPORT_PRIVATE
#endif  // defined(SYNC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SYNC_IMPLEMENTATION)
#define SYNC_EXPORT __attribute__((visibility("default")))
#define SYNC_EXPORT_PRIVATE __attribute__((visibility("default")))
#elif defined(SYNC_TEST)
#define SYNC_EXPORT
#define SYNC_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define SYNC_EXPORT
#define SYNC_EXPORT_PRIVATE
#endif  // defined(SYNC_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define SYNC_EXPORT
#define SYNC_EXPORT_PRIVATE
#endif

#endif  // SYNC_SYNC_EXPORT_H_
