// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// limited to top chrome UI.

#ifndef CHROME_BROWSER_UI_UI_FEATURES_H_
#define CHROME_BROWSER_UI_UI_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kAnimatedAvatarButton;

extern const base::Feature kDragToPinTabs;

extern const base::Feature kEvDetailsInPageInfo;

extern const base::Feature kExtensionsToolbarMenu;

extern const base::Feature kScrollableTabStrip;

extern const base::Feature kShowSyncPausedReasonCookiesClearedOnExit;

extern const base::Feature kTabGroups;

extern const base::Feature kTabHoverCards;
extern const char kTabHoverCardsFeatureParameterName[];

extern const base::Feature kTabHoverCardImages;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const base::Feature kEnableDbusAndX11StatusIcons;
#endif

#if defined(OS_CHROMEOS)
extern const base::Feature kHiddenNetworkWarning;
#endif  // defined(OS_CHROMEOS)
}  // namespace features

#endif  // CHROME_BROWSER_UI_UI_FEATURES_H_
