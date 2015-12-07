// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_BASE_SYNC_EXPORT_H_
#define SYNC_BASE_SYNC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SYNC_IMPLEMENTATION)
#define SYNC_EXPORT __declspec(dllexport)
#else  // defined(SYNC_IMPLEMENTATION)
#define SYNC_EXPORT __declspec(dllimport)
#endif  // defined(SYNC_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(SYNC_IMPLEMENTATION)
#define SYNC_EXPORT __attribute__((visibility("default")))
#else  // defined(SYNC_IMPLEMENTATION)
#define SYNC_EXPORT
#endif  // defined(SYNC_IMPLEMENTATION)

#endif  // defined(WIN32)
#else  // defined(COMPONENT_BUILD)
#define SYNC_EXPORT
#endif  // defined(COMPONENT_BUILD)

#endif  // SYNC_BASE_SYNC_EXPORT_H_
