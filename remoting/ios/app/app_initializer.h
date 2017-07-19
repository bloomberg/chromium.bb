// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_APP_INITIALIZER_H_
#define REMOTING_IOS_APP_APP_INITIALIZER_H_

#import <UIKit/UIKit.h>

// This class is to allow different builds (Chromium vs internal) to do
// dependency injection before starting the app. Please see main.mm to see how
// it is used.
@interface AppInitializer : NSObject

+ (void)initializeApp;

@end

#endif  // REMOTING_IOS_APP_APP_INITIALIZER_H_
