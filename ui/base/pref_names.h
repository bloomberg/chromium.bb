// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREF_NAMES_H_
#define UI_BASE_PREF_NAMES_H_

#include "ui/base/ui_base_export.h"

namespace prefs {

// The application locale.
// DO NOT USE this locale directly: use language::ConverToActualLocale() after
// reading it to get the system locale.
// This pref stores the locale that the user selected, if applicable.
UI_BASE_EXPORT extern const char kApplicationLocale[];

}  // namespace prefs

#endif  // UI_BASE_PREF_NAMES_H_
