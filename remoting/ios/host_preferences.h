// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_HOST_PREFERENCES_H_
#define REMOTING_IOS_HOST_PREFERENCES_H_

#import <CoreData/CoreData.h>

// A HostPreferences contains details to negotiate and maintain a connection
// to a remote Chromoting host.  This is a entity in a backing store.  The
// implementation file is ChromotingModel.xcdatamodeld.  If this file is
// updated, also update the model. The model MUST be properly versioned to
// ensure backwards compatibility.
// https://developer.apple.com/library/ios/recipes/xcode_help-core_data_modeling_tool/Articles/creating_new_version.html
// Or the app must be uninstalled, and reinstalled which will erase the previous
// version of the backing store.
@interface HostPreferences : NSManagedObject

// Is a prompt is needed to reconnect or continue the connection to
// the host
@property(nonatomic, copy) NSNumber* askForPin;
// Several properties are populated from the jabber jump server
@property(nonatomic, copy) NSString* hostId;
// Supplied by client via UI interaction
@property(nonatomic, copy) NSString* hostPin;
@property(nonatomic, copy) NSString* pairId;
@property(nonatomic, copy) NSString* pairSecret;

@end

#endif  // REMOTING_IOS_HOST_PREFERENCES_H_