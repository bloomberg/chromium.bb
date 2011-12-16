// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBKIT_GLUE_EXPORT_H_
#define WEBKIT_GLUE_WEBKIT_GLUE_EXPORT_H_
#pragma once

#if 0 // defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEBKIT_GLUE_IMPLEMENTATION)
#define WEBKIT_GLUE_EXPORT __declspec(dllexport)
#else
#define WEBKIT_GLUE_EXPORT __declspec(dllimport)
#endif  // defined(WEBKIT_GLUE_IMPLEMENTATION)

#else // defined(WIN32)
#define WEBKIT_GLUE_EXPORT __attribute__((visibility("default")))
#endif

#else // defined(COMPONENT_BUILD)
#define WEBKIT_GLUE_EXPORT
#endif

#endif  // WEBKIT_GLUE_WEBKIT_GLUE_EXPORT_H_
