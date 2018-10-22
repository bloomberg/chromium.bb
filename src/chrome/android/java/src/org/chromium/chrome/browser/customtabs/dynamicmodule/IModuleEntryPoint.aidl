// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import org.chromium.chrome.browser.customtabs.dynamicmodule.IActivityDelegate;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IActivityHost;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IModuleHost;

interface IModuleEntryPoint {
  void init(in IModuleHost moduleHost) = 0;
  int getModuleVersion() = 1;
  int getMinimumHostVersion() = 2;
  IActivityDelegate createActivityDelegate(in IActivityHost activityHost) = 3;
  void onDestroy() = 4;
}
