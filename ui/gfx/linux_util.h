// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_UTIL_H_
#define UI_GFX_LINUX_UTIL_H_
#pragma once

#include <stdint.h>

#include <string>

#include "ui/base/ui_export.h"

namespace gfx {

// Change windows accelerator style to GTK style. (GTK uses _ for
// accelerators.  Windows uses & with && as an escape for &.)
UI_EXPORT std::string ConvertAcceleratorsFromWindowsStyle(
    const std::string& label);

// Removes the "&" accelerators from a Windows label.
UI_EXPORT std::string RemoveWindowsStyleAccelerators(const std::string& label);

// Escapes "&" characters by doubling them so that later calling
// ConvertAcceleratorsFromWindowsStyle() will return the original string (except
// with "_" characters doubled, to escape them for GTK).
UI_EXPORT std::string EscapeWindowsStyleAccelerators(const std::string& label);

// Makes a copy of |pixels| with the ordering changed from BGRA to RGBA.
// The caller is responsible for free()ing the data. If |stride| is 0, it's
// assumed to be 4 * |width|.
uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride);

}  // namespace gfx

#endif  // UI_GFX_LINUX_UTIL_H_
