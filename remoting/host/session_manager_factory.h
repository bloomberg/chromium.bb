// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SESSION_MANAGER_FACTORY_H_
#define REMOTING_HOST_SESSION_MANAGER_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

class SignalStrategy;

namespace protocol {
struct NetworkSettings;
class SessionManager;
}  // namespace protocol

scoped_ptr<protocol::SessionManager> CreateHostSessionManager(
    SignalStrategy* signal_strategy,
    const protocol::NetworkSettings& network_settings,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter);

}  // namespace remoting

#endif  // REMOTING_HOST_SESSION_MANAGER_FACTORY_H_
