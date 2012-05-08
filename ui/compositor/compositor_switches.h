// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
#define UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
#pragma once

#include "ui/compositor/compositor_export.h"

namespace switches {

COMPOSITOR_EXPORT extern const char kDisableTestCompositor[];
COMPOSITOR_EXPORT extern const char kDisableUIVsync[];
COMPOSITOR_EXPORT extern const char kUIEnableDIP[];
COMPOSITOR_EXPORT extern const char kUIEnablePartialSwap[];
COMPOSITOR_EXPORT extern const char kUIShowFPSCounter[];
COMPOSITOR_EXPORT extern const char kUIShowLayerBorders[];
COMPOSITOR_EXPORT extern const char kUIShowLayerTree[];
COMPOSITOR_EXPORT extern const char kUIEnablePerTilePainting[];

}  // namespace switches

#endif  // UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
