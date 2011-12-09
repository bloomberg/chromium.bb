// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_COMPOSITOR_SETUP_H_
#define UI_GFX_COMPOSITOR_COMPOSITOR_SETUP_H_
#pragma once

#include "ui/gfx/compositor/compositor_export.h"

namespace ui {

// Configures the compositor in such a way that it doesn't render anything.
// Does nothing on platforms that aren't using the compositor.
#if !defined(VIEWS_COMPOSITOR)
// To centralize the ifdef to this file we define the function as doing nothing
// on all platforms that don't use a compositor.
COMPOSITOR_EXPORT void SetupTestCompositor() {}
#else
COMPOSITOR_EXPORT void SetupTestCompositor();

// Disables the test compositor so that the normal compositor is used.
COMPOSITOR_EXPORT void DisableTestCompositor();
#endif

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_COMPOSITOR_SETUP_H_
