// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_ENDPOINT_H_
#define NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_ENDPOINT_H_

#include "net/third_party/quic/core/quic_alarm_factory.h"
#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/platform/api/quic_clock.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/quartc/quartc_factory.h"

namespace quic {

// Private implementation of QuartcEndpoint.  Enables different implementations
// for client and server endpoints.
class QuartcEndpointImpl {
 public:
  virtual ~QuartcEndpointImpl() = default;
};

// Endpoint (client or server) in a peer-to-peer Quartc connection.
class QuartcEndpoint {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when an endpoint creates a new session, before any packets are
    // processed or sent.  The callee should perform any additional
    // configuration required, such as setting a session delegate, before
    // returning.  |session| is owned by the endpoint, but remains safe to use
    // until another call to |OnSessionCreated| occurs, at which point previous
    // session is destroyed.
    virtual void OnSessionCreated(QuartcSession* session) = 0;

    // Called if the endpoint fails to establish a session after a call to
    // Connect.  (The most likely cause is a network idle timeout.)
    virtual void OnConnectError(QuicErrorCode error,
                                const QuicString& error_details) = 0;
  };

  // |alarm_factory| and |clock| are owned by the caller and must outlive the
  // endpoint.
  QuartcEndpoint(QuicAlarmFactory* alarm_factory,
                 const QuicClock* clock,
                 Delegate* delegate);

  // Connects the endpoint using the given session config.  After |Connect| is
  // called, the endpoint will asynchronously create a session, then call
  // |Delegate::OnSessionCreated|.
  void Connect(const QuartcSessionConfig& config);

 private:
  // Implementation of QuicAlarmFactory used by this endpoint.  Unowned.
  QuicAlarmFactory* alarm_factory_;

  // Implementation of QuicClock used by this endpoint.  Unowned.
  const QuicClock* clock_;

  // Delegate which receives callbacks for newly created sessions.
  Delegate* delegate_;

  // Implementation of the endpoint.  Created when Connect() is called.  This
  // indirection enables different implementations for client and server
  // endpoints.
  std::unique_ptr<QuartcEndpointImpl> impl_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_ENDPOINT_H_
