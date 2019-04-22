// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"

#import <EarlGrey/EarlGrey.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ScopedSynchronizationDisabler::ScopedSynchronizationDisabler()
    : saved_eg_synchronization_enabled_value_(GetEgSynchronizationEnabled()) {
  SetEgSynchronizationEnabled(NO);
}

ScopedSynchronizationDisabler::~ScopedSynchronizationDisabler() {
  SetEgSynchronizationEnabled(saved_eg_synchronization_enabled_value_);
}

bool ScopedSynchronizationDisabler::GetEgSynchronizationEnabled() {
  return [[GREYConfiguration sharedInstance]
      boolValueForConfigKey:kGREYConfigKeySynchronizationEnabled];
}

void ScopedSynchronizationDisabler::SetEgSynchronizationEnabled(BOOL flag) {
  [[GREYConfiguration sharedInstance]
          setValue:@(flag)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}
