// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_CONSTANTS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Key in NSUserDefaults that contains the managed app configuration.
extern NSString* const kPolicyLoaderIOSConfigurationKey;

// Key that controls whether policy data is loaded from NSUserDefaults.
extern NSString* const kPolicyLoaderIOSLoadPolicyKey;

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_IOS_CONSTANTS_H_
