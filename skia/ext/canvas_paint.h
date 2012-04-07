// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_H_
#define SKIA_EXT_CANVAS_PAINT_H_
#pragma once

// This file provides an easy way to include the appropriate CanvasPaint
// header file on your platform.

#if defined(WIN32)
#include "skia/ext/canvas_paint_win.h"
#elif defined(__APPLE__)
#include "skia/ext/canvas_paint_mac.h"
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__sun)
#if defined(TOOLKIT_GTK)
#include "skia/ext/canvas_paint_gtk.h"
#else
#error "No canvas paint for this platform"
#endif
#endif

#endif  // SKIA_EXT_CANVAS_PAINT_H_
