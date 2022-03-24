// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_UI_COLOR_MIXER_H_
#define UI_COLOR_UI_COLOR_MIXER_H_

namespace ui {

class ColorProvider;

// Adds a color mixer to |provider| that combine the above color sets with
// recipes as necessary to produce all colors needed by ui/.
void AddUiColorMixer(ColorProvider* provider,
                     bool dark_window,
                     bool high_contrast);

}  // namespace ui

#endif  // UI_COLOR_CORE_DEFAULT_COLOR_MIXER_H_