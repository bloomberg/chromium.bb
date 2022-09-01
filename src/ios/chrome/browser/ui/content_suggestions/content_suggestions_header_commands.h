// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_COMMANDS_H_

// Commands protocol allowing the ContentSuggestionsHeaderViewController to
// interact with the coordinator layer.
@protocol ContentSuggestionsHeaderCommands

// Informs the receiver that the ContentSuggestionsHeaderViewController's size
// has changed.
- (void)updateForHeaderSizeChange;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_COMMANDS_H_
