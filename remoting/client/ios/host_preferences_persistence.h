// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_HOST_PREFERENCES_PERSISTENCE_H_
#define REMOTING_CLIENT_IOS_HOST_PREFERENCES_PERSISTENCE_H_

#import <CoreData/CoreData.h>

// Methods used to store and recall Host Prefrences on the keychain.
// Used to cache data for quicker connection to previously fetched host data.
namespace remoting {
namespace ios {

NSError* WriteHostPreferencesToKeychain(NSData* data);
NSData* ReadHostPreferencesFromKeychain();

}  // namespace ios
}  // namespace remoting

#endif  // REMOTING_CLIENT_IOS_HOST_PREFERENCES_PERSISTENCE_H_
