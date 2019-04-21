// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_FACTORY_H_
#define CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_FACTORY_H_

#include <memory>

#include "base/callback.h"

class SyncWebSocket;
class URLRequestContextGetter;

typedef base::Callback<std::unique_ptr<SyncWebSocket>()> SyncWebSocketFactory;

SyncWebSocketFactory CreateSyncWebSocketFactory(
    URLRequestContextGetter* getter);

#endif  // CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_FACTORY_H_
