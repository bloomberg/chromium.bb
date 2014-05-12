// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SANDBOX_EXPORT_H_
#define SANDBOX_SANDBOX_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SANDBOX_IMPLEMENTATION)
#define SANDBOX_EXPORT __declspec(dllexport)
#else
#define SANDBOX_EXPORT __declspec(dllimport)
#endif  // defined(GFX_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(SANDBOX_IMPLEMENTATION)
#define SANDBOX_EXPORT __attribute__((visibility("default")))
#define SANDBOX_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define SANDBOX_EXPORT
#define SANDBOX_EXPORT_PRIVATE
#endif  // defined(SANDBOX_IMPLEMENTATION)

#endif  // defined(WIN32)
#else  // defined(COMPONENT_BUILD)

#define SANDBOX_EXPORT
#define SANDBOX_EXPORT_PRIVATE

#endif  // defined(COMPONENT_BUILD)

#endif  // SANDBOX_SANDBOX_EXPORT_H_
