// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_VIEW_UTILS_H_
#define REMOTING_IOS_APP_VIEW_UTILS_H_

#import <UIKit/UIKit.h>

namespace remoting {

// Returns the current topmost presenting view controller of the app.
UIViewController* TopPresentingVC();

// Returns the proper safe area layout guide for iOS 11; returns a dumb layout
// guide for older OS versions that exactly matches the anchors of the view.
UILayoutGuide* SafeAreaLayoutGuideForView(UIView* view);

}  // namespace remoting

#endif  // REMOTING_IOS_APP_VIEW_UTILS_H_
