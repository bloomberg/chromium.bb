// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
#define REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_

#include <list>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport.h"

namespace remoting {

class SignalStrategy;
class UrlRequestFactory;

namespace protocol {

class PortAllocatorFactory;
class IceConfigRequest;

// TransportContext is responsible for storing all parameters required for
// P2P transport initialization. It's also responsible for fetching STUN and
// TURN configuration.
class TransportContext : public base::RefCountedThreadSafe<TransportContext> {
 public:
  typedef base::Callback<void(const IceConfig& ice_config)>
      GetIceConfigCallback;

  static scoped_refptr<TransportContext> ForTests(TransportRole role);

  TransportContext(SignalStrategy* signal_strategy,
                   scoped_ptr<PortAllocatorFactory> port_allocator_factory,
                   scoped_ptr<UrlRequestFactory> url_request_factory,
                   const NetworkSettings& network_settings,
                   TransportRole role);

  // Prepares fresh JingleInfo. It may be called while connection is being
  // negotiated to minimize the chance that the following GetIceConfig() will
  // be blocking.
  void Prepare();

  // Requests fresh STUN and TURN information.
  void GetIceConfig(const GetIceConfigCallback& callback);

  PortAllocatorFactory* port_allocator_factory() {
    return port_allocator_factory_.get();
  }
  UrlRequestFactory* url_request_factory() {
    return url_request_factory_.get();
  }
  const NetworkSettings& network_settings() const { return network_settings_; }
  TransportRole role() const { return role_; }

 private:
  friend class base::RefCountedThreadSafe<TransportContext>;

  ~TransportContext();

  void EnsureFreshJingleInfo();
  void OnIceConfig(const IceConfig& ice_config);

  SignalStrategy* signal_strategy_;
  scoped_ptr<PortAllocatorFactory> port_allocator_factory_;
  scoped_ptr<UrlRequestFactory> url_request_factory_;
  NetworkSettings network_settings_;
  TransportRole role_;

  scoped_ptr<IceConfigRequest> ice_config_request_;

  IceConfig ice_config_;

  // When there is an active |jingle_info_request_| stores list of callbacks to
  // be called once the request is finished.
  std::list<GetIceConfigCallback> pending_ice_config_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TransportContext);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONTEXT_H_
