// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_export.h"

namespace features {

#if defined(OS_WIN)
UI_BASE_EXPORT extern const base::Feature kDirectManipulationStylus;
#endif  // defined(OS_WIN)

UI_BASE_EXPORT extern const base::Feature kSecondaryUiMd;

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
