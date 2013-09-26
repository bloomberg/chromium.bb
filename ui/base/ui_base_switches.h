// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_

#include "base/compiler_specific.h"
#include "ui/base/ui_export.h"

namespace switches {

UI_EXPORT extern const char kDisableDwmComposition[];
UI_EXPORT extern const char kDisableTouchAdjustment[];
UI_EXPORT extern const char kDisableTouchDragDrop[];
UI_EXPORT extern const char kDisableTouchEditing[];
UI_EXPORT extern const char kDisableViewsTextfield[];
UI_EXPORT extern const char kEnableTouchDragDrop[];
UI_EXPORT extern const char kEnableTouchEditing[];
UI_EXPORT extern const char kEnableViewsTextfield[];
UI_EXPORT extern const char kHighlightMissingScaledResources[];
UI_EXPORT extern const char kLang[];
UI_EXPORT extern const char kLocalePak[];
UI_EXPORT extern const char kNoMessageBox[];
UI_EXPORT extern const char kTouchOptimizedUI[];
UI_EXPORT extern const char kTouchOptimizedUIAuto[];
UI_EXPORT extern const char kTouchOptimizedUIDisabled[];
UI_EXPORT extern const char kTouchOptimizedUIEnabled[];
UI_EXPORT extern const char kTouchSideBezels[];

#if defined(USE_XI2_MT)
UI_EXPORT extern const char kTouchCalibration[];
#endif

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
