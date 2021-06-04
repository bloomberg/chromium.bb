// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_LAYOUT_GUIDE_NAMES_H_
#define IOS_CHROME_BROWSER_UI_UTIL_LAYOUT_GUIDE_NAMES_H_

#import <UIKit/UIKit.h>

typedef NSString GuideName;

// The list of well-known UILayoutGuides.  When adding a new guide to the app,
// create a constant for it below. Because these constants are in the global
// namespace, all guide names should end in 'Guide', for clarity.

// A guide that is constrained to match the frame of the tab's content area.
extern GuideName* const kContentAreaGuide;
// A guide that is constrained to match the frame of the primary toolbar. This
// follows the frame of the primary toolbar even when that frame shrinks due to
// fullscreen. It does not include the tab strip on iPad.
extern GuideName* const kPrimaryToolbarGuide;
// A guide that is constrained to match the frame of the secondary toolbar.
extern GuideName* const kSecondaryToolbarGuide;
// A guide that is constrained to match the frame the secondary toolbar would
// have if fullscreen was disabled.
extern GuideName* const kSecondaryToolbarNoFullscreenGuide;
// A guide that is constrainted to match the frame of the displayedBadge in the
// Badge View.
extern GuideName* const kBadgeOverflowMenuGuide;
// A guide that is constrained to match the frame of the omnibox.
extern GuideName* const kOmniboxGuide;
// A guide that is constrained to match the frame of the leading image view in
// the omnibox.
extern GuideName* const kOmniboxLeadingImageGuide;
// A guide that is constrainted to match the frame of the text field in the
// omnibox.
extern GuideName* const kOmniboxTextFieldGuide;
// A guide that is constrained to match the frame of the back button's image.
extern GuideName* const kBackButtonGuide;
// A guide that is constrained to match the frame of the forward button's image.
extern GuideName* const kForwardButtonGuide;
// A guide that is constrained to match the frame of the NewTab button.
extern GuideName* const kNewTabButtonGuide;
// A guide that is constrained to match the frame of the TabSwitcher button's
// image.
extern GuideName* const kTabSwitcherGuide;
// A guide that is constrained to match the frame of the ToolsMenu button.
extern GuideName* const kToolsMenuGuide;
// A guide that is constrained to match the frame of the translate infobar
// options button.
extern GuideName* const kTranslateInfobarOptionsGuide;
// A guide that is constrained to match the frame of the last-tapped voice
// search button.
extern GuideName* const kVoiceSearchButtonGuide;
// A guide that is constrained to match the frame of the Discover feed header's
// top-level menu button.
extern GuideName* const kDiscoverFeedHeaderMenuGuide;
// A guide that is constrained to match the frame of the location view in the
// primary toolbar (i.e. the Address Bar).
extern GuideName* const kPrimaryToolbarLocationViewGuide;
// A guide that is constrained to match the frame of the bottom toolbar in the
// tab grid.
extern GuideName* const kTabGridBottomToolbarGuide;

#endif  // IOS_CHROME_BROWSER_UI_UTIL_LAYOUT_GUIDE_NAMES_H_
