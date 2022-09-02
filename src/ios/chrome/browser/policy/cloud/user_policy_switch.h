// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SWITCH_H_
#define IOS_CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SWITCH_H_

namespace policy {

extern const char kEnableUserPolicy[];

// Enables User Policy with the commandline switch.
void EnableUserPolicy();

// True if User Policy is enabled.
bool IsUserPolicyEnabled();

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SWITCH_H_
