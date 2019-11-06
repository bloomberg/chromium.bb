// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_AR_QUICK_LOOK_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_AR_QUICK_LOOK_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class ARQuickLookTabHelper;

@protocol ARQuickLookTabHelperDelegate

// Called to preview the downloaded USDZ format file |fileURL| points to.
// |fileURL| cannot be nil.
- (void)ARQuickLookTabHelper:(ARQuickLookTabHelper*)tabHelper
    didFinishDowloadingFileWithURL:(NSURL*)fileURL;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_AR_QUICK_LOOK_TAB_HELPER_DELEGATE_H_
