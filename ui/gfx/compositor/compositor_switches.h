// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_SWITCHES_H_
#define UI_GFX_COMPOSITOR_SWITCHES_H_
#pragma once

#include "ui/gfx/compositor/compositor_export.h"

namespace switches {

COMPOSITOR_EXPORT extern const char kDisableUIVsync[];
COMPOSITOR_EXPORT extern const char kEnableCompositorOverdrawDebugging[];
COMPOSITOR_EXPORT extern const char kUIShowFPSCounter[];
COMPOSITOR_EXPORT extern const char kUIShowLayerBorders[];
COMPOSITOR_EXPORT extern const char kUIShowLayerTree[];

}  // namespace switches

#endif  // UI_GFX_COMPOSITOR_SWITCHES_H_
