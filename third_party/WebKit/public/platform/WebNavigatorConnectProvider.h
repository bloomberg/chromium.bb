// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNavigatorConnectProvider_h
#define WebNavigatorConnectProvider_h

#include "WebCallbacks.h"

namespace blink {

class WebMessagePortChannel;
class WebString;
class WebURL;

typedef WebCallbacks<void, void> WebNavigatorConnectCallbacks;
typedef WebCallbacks<WebMessagePortChannel, void> WebNavigatorConnectPortCallbacks;

class WebNavigatorConnectProvider {
public:
    virtual ~WebNavigatorConnectProvider() { }

    // Initiates a connection from the given origin to the given URL. When
    // successful the service can communicate with the client over the given
    // channel. The origin isn't passed as WebSecurityOrigin because that would
    // be a layering violation (platform/ code shouldn't depend on web/ code).
    // Ownership of the WebMessagePortChannel and WebNavigatorConnectCallbacks
    // objects are both transferred to the provider.
    virtual void connect(const WebURL&, const WebString& origin, WebNavigatorConnectPortCallbacks*) { }
};

} // namespace blink

#endif // WebNavigatorConnectProvider_h
