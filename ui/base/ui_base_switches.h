// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_export.h"
#include "ui/base/ui_features.h"

namespace features {

UI_BASE_EXPORT extern const base::Feature kEnableFloatingVirtualKeyboard;

}  // namespace features

namespace switches {

#if defined(OS_MACOSX) && !defined(OS_IOS)
UI_BASE_EXPORT extern const char kDisableAVFoundationOverlays[];
UI_BASE_EXPORT extern const char kDisableMacOverlays[];
UI_BASE_EXPORT extern const char kDisableRemoteCoreAnimation[];
UI_BASE_EXPORT extern const char kShowMacOverlayBorders[];
#endif

#if defined(OS_WIN)
UI_BASE_EXPORT extern const char kDisableMergeKeyCharEvents[];
UI_BASE_EXPORT extern const char kEnableMergeKeyCharEvents[];
#endif

UI_BASE_EXPORT extern const char kDisableCompositedAntialiasing[];
UI_BASE_EXPORT extern const char kDisableDwmComposition[];
UI_BASE_EXPORT extern const char kDisableTouchAdjustment[];
UI_BASE_EXPORT extern const char kDisableTouchDragDrop[];
UI_BASE_EXPORT extern const char kEnableDrawOcclusion[];
UI_BASE_EXPORT extern const char kEnableTouchDragDrop[];
UI_BASE_EXPORT extern const char kGlCompositedOverlayCandidateQuadBorder[];
UI_BASE_EXPORT extern const char kLang[];
UI_BASE_EXPORT extern const char kMaterialDesignInkDropAnimationSpeed[];
UI_BASE_EXPORT extern const char kMaterialDesignInkDropAnimationSpeedFast[];
UI_BASE_EXPORT extern const char kMaterialDesignInkDropAnimationSpeedSlow[];
UI_BASE_EXPORT extern const char kShowOverdrawFeedback[];
UI_BASE_EXPORT extern const char kSlowDownCompositingScaleFactor[];
UI_BASE_EXPORT extern const char kTopChromeMD[];
UI_BASE_EXPORT extern const char kTopChromeMDMaterial[];
UI_BASE_EXPORT extern const char kTopChromeMDMaterialAuto[];
UI_BASE_EXPORT extern const char kTopChromeMDMaterialHybrid[];
UI_BASE_EXPORT extern const char kTopChromeMDNonMaterial[];
UI_BASE_EXPORT extern const char kUIDisablePartialSwap[];
UI_BASE_EXPORT extern const char kUseSkiaRenderer[];

// Test related.
UI_BASE_EXPORT extern const char kDisallowNonExactResourceReuse[];
UI_BASE_EXPORT extern const char kMangleLocalizedStrings[];

#if defined(USE_AURA)
UI_BASE_EXPORT extern const char kMus[];
UI_BASE_EXPORT extern const char kMusHostVizValue[];
#endif

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
