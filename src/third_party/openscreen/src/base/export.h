// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_EXPORT_H_
#define BASE_EXPORT_H_

#if defined(WIN32)

#if defined(OPENSCREEN_SHARED_IMPLEMENTATION)
#define OPENSCREEN_EXPORT __declspec(dllexport)
#else
#define OPENSCREEN_EXPORT __declspec(dllimport)
#endif  // defined(OPENSCREEN_SHARED_IMPLEMENTATION)

#else

#if defined(OPENSCREEN_SHARED_IMPLEMENTATION)
#define OPENSCREEN_EXPORT __attribute__((visibility("default")))
#else
#define OPENSCREEN_EXPORT
#endif  // defined(OPENSCREEN_SHARED_IMPLEMENTATION)

#endif  // defined(WIN32)

#endif  // BASE_EXPORT_H_
