// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_UI_API_H_
#define UI_UI_API_H_
#pragma once

// Defines UI_API so that funtionality implemented by the UI module can be
// exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(UI_IMPLEMENTATION)
#define UI_API __declspec(dllexport)
#else
#define UI_API __declspec(dllimport)
#endif  // defined(UI_BASE_IMPLEMENTATION)

#else  // defined(WIN32)
#define UI_API __attribute__((visibility("default")))
#endif

#else  /// defined(COMPONENT_BUILD)
#define UI_API
#endif

#endif  // UI_UI_API_H_
