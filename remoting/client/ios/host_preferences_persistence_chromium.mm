// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/client/ios/host_preferences_persistence.h"

#import "base/logging.h"

namespace remoting {
namespace ios {

// TODO(nicholss): It might be useful to save |data| in a static variable,
// which is then returned from ReadHostPreferencesFromKeychain(). This would
// allow to test pairing, even though the pairing info is not persisted when
// the app is restarted.

NSError* WriteHostPreferencesToKeychain(NSData* data) {
  NOTIMPLEMENTED();
  return nil;
}

NSData* ReadHostPreferencesFromKeychain() {
  NOTIMPLEMENTED();
  return nil;
}

}  // namespace ios
}  // namespace remoting
