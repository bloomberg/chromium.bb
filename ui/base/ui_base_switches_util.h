// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_SWITCHES_UTIL_H_
#define UI_BASE_UI_BASE_SWITCHES_UTIL_H_

#include "ui/base/ui_base_export.h"

namespace switches {

UI_BASE_EXPORT bool IsLinkDisambiguationPopupEnabled();
UI_BASE_EXPORT bool IsTouchDragDropEnabled();

// Returns whether mus is hosting viz. Mus is hosting viz only if
// --mus-hosting-viz is set.
UI_BASE_EXPORT bool IsMusHostingViz();

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_UTIL_H_
