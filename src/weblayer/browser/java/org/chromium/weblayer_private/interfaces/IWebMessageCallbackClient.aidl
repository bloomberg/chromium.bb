// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IWebMessageReplyProxy;

interface IWebMessageCallbackClient {
  void onNewReplyProxy(in IWebMessageReplyProxy proxy,
                       in int proxyId,
                       in boolean isMainFrame,
                       in String sourceOrigin) = 0;
  void onPostMessage(in int proxyId, in String message) = 1;
  void onReplyProxyDestroyed(in int proxyId) = 2;
}
