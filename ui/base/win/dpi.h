// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_DPI_H_
#define UI_BASE_WIN_DPI_H_
#pragma once

#include "ui/gfx/size.h"
#include "ui/base/ui_export.h"

namespace ui {

UI_EXPORT gfx::Size GetDPI();

// Gets the scale factor of the display. For example, if the display DPI is
// 96 then the scale factor is 1.0.
UI_EXPORT float GetDPIScale();

UI_EXPORT bool IsInHighDPIMode();

UI_EXPORT void EnableHighDPISupport();

}  // namespace ui

#endif  // UI_BASE_WIN_DPI_H_
