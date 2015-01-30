// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_FONT_HELPER_CHROMEOS_H
#define UI_BASE_FONT_HELPER_CHROMEOS_H

#include <string>

namespace ui {

// This is a temporary helper function for the Roboto UI font experiment.
// It'll go away once the experiment is over.
// See http://crbug.com/434400 and http://crbug.com/448948 for more details.
//
// "Noto Sans UI" was the primary UI font (both native and web) in most
// locales and we're replacing it with 'Roboto'. For now 'Roboto' is enabled by
// default and can be disabled by setting 'disable-roboto-font-ui' flag to on.
void ReplaceNotoSansWithRobotoIfEnabled(std::string* font_family);

}  // namespace ui

#endif  // UI_BASE_FONT_HELPER_CHROMEOS_H
