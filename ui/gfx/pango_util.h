// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PANGO_UTIL_H_
#define UI_GFX_PANGO_UTIL_H_
#pragma once

#include <pango/pango.h>
#include "base/i18n/rtl.h"
#include "base/string16.h"

// TODO(xji): move other pango related functions from gtk_util to here.

namespace gfx {

class Font;

// Setup pango layout |layout|, including set layout text as |text|, font
// description based on |font|, width as |width| in PANGO_SCALE for RTL lcoale,
// and set up whether auto-detect directionality, alignment, ellipsis, word
// wrapping, resolution etc.
void SetupPangoLayout(PangoLayout* layout,
                      const string16& text,
                      const gfx::Font& font,
                      int width,
                      base::i18n::TextDirection text_direction,
                      int flags);
}  // namespace gfx

#endif  // UI_GFX_PANGO_UTIL_H_
