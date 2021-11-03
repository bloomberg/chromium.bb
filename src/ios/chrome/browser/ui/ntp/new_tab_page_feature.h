// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"

// Feature flag to enable showing a live preview for discover feed when opening
// the feed context menu.
extern const base::Feature kEnableDiscoverFeedPreview;

// Feature flag to enable improving the usage of memory of the NTP.
extern const base::Feature kEnableNTPMemoryEnhancement;

// Feature flag to enable static resource serving for the discover feed.
extern const base::Feature kEnableDiscoverFeedStaticResourceServing;

// Feature flag to enable discofeed endpoint for the discover feed.
extern const base::Feature kEnableDiscoverFeedDiscoFeedEndpoint;

// Feature flag to enable static resource serving for the discover feed.
extern const base::Feature kEnableDiscoverFeedStaticResourceServing;

// A parameter to indicate whether Reconstructed Templates is enabled for static
// resource serving.
extern const char kDiscoverFeedSRSReconstructedTemplatesEnabled[];

// A parameter to indicate whether Preload Templates is enabled for static
// resource serving.
extern const char kDiscoverFeedSRSPreloadTemplatesEnabled[];

// Feature flag to fix the NTP view hierarchy if it is broken before applying
// constraints.
// TODO(crbug.com/1262536): Remove this when it is fixed.
extern const base::Feature kNTPViewHierarchyRepair;

// Whether the Discover feed content preview is shown in the context menu.
bool IsDiscoverFeedPreviewEnabled();

// Whether the discover feed appflows are enabled.
bool IsDiscoverFeedAppFlowsEnabled();

// Whether the NTP view hierarchy repair is enabled.
bool IsNTPViewHierarchyRepairEnabled();

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
