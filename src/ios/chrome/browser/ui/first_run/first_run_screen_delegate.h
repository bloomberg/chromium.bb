// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_DELEGATE_H_

// The delegate for transferring between screens.
@protocol FirstRunScreenDelegate <NSObject>

// Called when one screen finished presenting.
- (void)willFinishPresenting;

// Called when user want to skip all screens after.
- (void)skipAll;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_DELEGATE_H_
