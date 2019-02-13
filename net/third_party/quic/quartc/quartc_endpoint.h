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

// Endpoint (client or server) in a peer-to-peer Quartc connection.
class QUIC_EXPORT_PRIVATE QuartcEndpoint {
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
  friend class CreateSessionDelegate;
  class CreateSessionDelegate : public QuicAlarm::Delegate {
   public:
    CreateSessionDelegate(QuartcEndpoint* endpoint) : endpoint_(endpoint) {}

    void OnAlarm() override { endpoint_->OnCreateSessionAlarm(); };

   private:
    QuartcEndpoint* endpoint_;
  };

  // Callback which occurs when |create_session_alarm_| fires.
  void OnCreateSessionAlarm();

  // Implementation of QuicAlarmFactory used by this endpoint.  Unowned.
  QuicAlarmFactory* alarm_factory_;

  // Implementation of QuicClock used by this endpoint.  Unowned.
  const QuicClock* clock_;

  // Delegate which receives callbacks for newly created sessions.
  Delegate* delegate_;

  // Alarm for creating sessions asynchronously.  The alarm is set when
  // Connect() is called.  When it fires, the endpoint creates a session and
  // calls the delegate.
  std::unique_ptr<QuicAlarm> create_session_alarm_;

  // QuartcFactory used by this endpoint to create sessions.  This is an
  // implementation detail of the QuartcEndpoint, and will eventually be
  // replaced by a dispatcher (for servers) or version-negotiation agent (for
  // clients).
  std::unique_ptr<QuartcFactory> factory_;

  // Config to be used for new sessions.
  QuartcSessionConfig config_;

  // The currently-active session.  Nullptr until |Connect| and
  // |Delegate::OnSessionCreated| are called.
  std::unique_ptr<QuartcSession> session_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_QUARTC_QUARTC_ENDPOINT_H_
