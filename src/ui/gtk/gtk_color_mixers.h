// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_COLOR_MIXERS_H_
#define UI_GTK_GTK_COLOR_MIXERS_H_

#include "ui/color/color_provider_manager.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace gtk {

void AddGtkNativeCoreColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderManager::Key& key);

}  // namespace gtk

#endif  // UI_GTK_GTK_COLOR_MIXERS_H_
