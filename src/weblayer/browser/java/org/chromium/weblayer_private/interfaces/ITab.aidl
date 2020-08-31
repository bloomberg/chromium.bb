// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFindInPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IMediaCaptureCallbackClient;
import org.chromium.weblayer_private.interfaces.INavigationController;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITabClient;

interface ITab {
  void setClient(in ITabClient client) = 0;

  INavigationController createNavigationController(in INavigationControllerClient client) = 1;

  // Deprecated, use Profile.setDownloadCallbackClient.
  void setDownloadCallbackClient(IDownloadCallbackClient client) = 2;

  void setErrorPageCallbackClient(IErrorPageCallbackClient client) = 3;

  void setFullscreenCallbackClient(in IFullscreenCallbackClient client) = 4;

  void executeScript(in String script, boolean useSeparateIsolate, in IObjectWrapper callback) = 5;

  void setNewTabsEnabled(in boolean enabled) = 6;

  // Returns a unique identifier for this Tab. The id is *not* unique across
  // restores. The id is intended for the client library to avoid creating duplicate client objects
  // for the same ITab.
  int getId() = 7;

  boolean setFindInPageCallbackClient(IFindInPageCallbackClient client) = 8;
  void findInPage(in String searchText, boolean forward) = 9;

  // And and removed in 82; superseded by dismissTransientUi().
  // void dismissTabModalOverlay() = 10;
  void dispatchBeforeUnloadAndClose() = 11;

  boolean dismissTransientUi() = 12;

  String getGuid() = 13;

  void setMediaCaptureCallbackClient(in IMediaCaptureCallbackClient client) = 14;
  void stopMediaCapturing() = 15;

  // Added in 84
  void captureScreenShot(in float scale, in IObjectWrapper resultCallback) = 16;
}
