// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/base/ui_export.h"

namespace switches {

UI_EXPORT extern const char kDefaultDeviceScaleFactor[];
UI_EXPORT extern const char kDisableTouchCalibration[];
UI_EXPORT extern const char kEnableTouchEvents[];
UI_EXPORT extern const char kLang[];
UI_EXPORT extern const char kLocalePak[];
UI_EXPORT extern const char kNoMessageBox[];
UI_EXPORT extern const char kTouchOptimizedUI[];

#if defined(OS_MACOSX)
// TODO(kbr): remove this and the associated old code path:
// http://crbug.com/105344
// This isn't really the right place for this switch, but is the most
// convenient place where it can be shared between
// src/webkit/plugins/npapi/ and src/content/plugin/ .
UI_EXPORT extern const char kDisableCompositedCoreAnimationPlugins[];
#endif

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
