// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.ITabClient;
import org.chromium.weblayer_private.aidl.IFullscreenCallbackClient;
import org.chromium.weblayer_private.aidl.INavigationController;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;

interface ITab {
  void setClient(in ITabClient client) = 0;

  INavigationController createNavigationController(in INavigationControllerClient client) = 1;

  void setDownloadCallbackClient(IDownloadCallbackClient client) = 2;

  void setFullscreenCallbackClient(in IFullscreenCallbackClient client) = 3;

  void executeScript(in String script, boolean useSeparateIsolate, in IObjectWrapper callback) = 4;

  void setNewTabsEnabled(in boolean enabled) = 5;

  // Returns a unique identifier for this Tab. The id is *not* unique across
  // restores. The id is intended for the client library to avoid creating duplicate client objects
  // for the same ITab.
  int getId() = 6;
}
