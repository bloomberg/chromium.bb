// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_VIEWS_API_H_
#define VIEWS_VIEWS_API_H_
#pragma once

// Defines VIEWS_API so that funtionality implemented by the UI module can be
// exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIEWS_IMPLEMENTATION)
#define VIEWS_API __declspec(dllexport)
#else
#define VIEWS_API __declspec(dllimport)
#endif  // defined(VIEWS_IMPLEMENTATION)

#else  // defined(WIN32)
#define VIEWS_API __attribute__((visibility("default")))
#endif

#else  /// defined(COMPONENT_BUILD)
#define VIEWS_API
#endif

#endif  // UI_UI_API_H_
