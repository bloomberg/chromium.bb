// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_UTILS_H_
#define UI_NATIVE_THEME_NATIVE_THEME_UTILS_H_

#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_id.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_export.h"

namespace ui {

// The following functions convert various values to strings intended for
// logging. Do not retain the results for longer than the scope in which these
// functions are called.

// Converts NativeTheme::ColorId.
base::StringPiece NATIVE_THEME_EXPORT
NativeThemeColorIdName(NativeTheme::ColorId color_id);

// Converts NativeTheme::ColorScheme.
base::StringPiece NATIVE_THEME_EXPORT
NativeThemeColorSchemeName(NativeTheme::ColorScheme color_scheme);

// Converts a NativeTheme::ColorId to a ColorPipeline ColorId.
absl::optional<ColorId> NATIVE_THEME_EXPORT
NativeThemeColorIdToColorId(NativeTheme::ColorId native_theme_color_id);

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_UTILS_H_
