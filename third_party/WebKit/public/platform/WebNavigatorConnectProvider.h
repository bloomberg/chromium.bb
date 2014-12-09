// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNavigatorConnectProvider_h
#define WebNavigatorConnectProvider_h

#include "WebCallbacks.h"

namespace blink {

class WebMessagePortChannel;
class WebURL;

class WebNavigatorConnectProvider {
public:
    virtual ~WebNavigatorConnectProvider() { }

    // Initiates a connection to the given URL. When succesfull the service
    // can communicate with the client over the given channel.
    // Ownership of the WebMessagePortChannel and WebCallbacks is transferred to the provider.
    virtual void connect(const WebURL&, WebMessagePortChannel*, WebCallbacks<void, void>*) { }
};

} // namespace blink

#endif // WebNavigatorConnectProvider_h
