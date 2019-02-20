// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_endpoint.h"
#include "net/third_party/quic/core/quic_version_manager.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/quartc/quartc_connection_helper.h"
#include "net/third_party/quic/quartc/quartc_crypto_helpers.h"
#include "net/third_party/quic/quartc/quartc_dispatcher.h"

namespace quic {

namespace {

// Wrapper around a QuicAlarmFactory which delegates to the wrapped factory.
// Usee to convert an unowned pointer into an owned pointer, so that the new
// "owner" does not delete the underlying factory.  Note that this is only valid
// when the unowned pointer is already guaranteed to outlive the new "owner".
class QuartcAlarmFactoryWrapper : public QuicAlarmFactory {
 public:
  explicit QuartcAlarmFactoryWrapper(QuicAlarmFactory* impl) : impl_(impl) {}

  QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) override;
  QuicArenaScopedPtr<QuicAlarm> CreateAlarm(
      QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
      QuicConnectionArena* arena) override;

 private:
  QuicAlarmFactory* impl_;
};

QuicAlarm* QuartcAlarmFactoryWrapper::CreateAlarm(
    QuicAlarm::Delegate* delegate) {
  return impl_->CreateAlarm(delegate);
}

QuicArenaScopedPtr<QuicAlarm> QuartcAlarmFactoryWrapper::CreateAlarm(
    QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
    QuicConnectionArena* arena) {
  return impl_->CreateAlarm(std::move(delegate), arena);
}

QuartcFactoryConfig CreateFactoryConfig(QuicAlarmFactory* alarm_factory,
                                        const QuicClock* clock) {
  QuartcFactoryConfig config;
  config.alarm_factory = alarm_factory;
  config.clock = clock;
  return config;
}

// Implementation of QuartcEndpoint which immediately (but asynchronously)
// creates a session by scheduling a QuicAlarm.  Only suitable for use with the
// client perspective.
class QuartcAlarmEndpointImpl : public QuartcEndpointImpl {
 public:
  QuartcAlarmEndpointImpl(QuicAlarmFactory* alarm,
                          const QuicClock* clock,
                          QuartcEndpoint::Delegate* delegate,
                          const QuartcSessionConfig& config);

 private:
  friend class CreateSessionDelegate;
  class CreateSessionDelegate : public QuicAlarm::Delegate {
   public:
    CreateSessionDelegate(QuartcAlarmEndpointImpl* endpoint)
        : endpoint_(endpoint) {}

    void OnAlarm() override { endpoint_->OnCreateSessionAlarm(); }

   private:
    QuartcAlarmEndpointImpl* endpoint_;
  };

  // Callback which occurs when |create_session_alarm_| fires.
  void OnCreateSessionAlarm();

  // Implementation of QuicAlarmFactory used by this endpoint.  Unowned.
  QuicAlarmFactory* alarm_factory_;

  // Implementation of QuicClock used by this endpoint.  Unowned.
  const QuicClock* clock_;

  // Delegate which receives callbacks for newly created sessions.
  QuartcEndpoint::Delegate* delegate_;

  // Config to be used for new sessions.
  QuartcSessionConfig config_;

  // Alarm for creating sessions asynchronously.  The alarm is set when
  // Connect() is called.  When it fires, the endpoint creates a session and
  // calls the delegate.
  std::unique_ptr<QuicAlarm> create_session_alarm_;

  // QuartcFactory used by this endpoint to create sessions.  This is an
  // implementation detail of the QuartcEndpoint, and will eventually be
  // replaced by a dispatcher (for servers) or version-negotiation agent (for
  // clients).
  std::unique_ptr<QuartcFactory> factory_;

  // The currently-active session.  Nullptr until |Connect| and
  // |Delegate::OnSessionCreated| are called.
  std::unique_ptr<QuartcSession> session_;
};

QuartcAlarmEndpointImpl::QuartcAlarmEndpointImpl(
    QuicAlarmFactory* alarm_factory,
    const QuicClock* clock,
    QuartcEndpoint::Delegate* delegate,
    const QuartcSessionConfig& config)
    : alarm_factory_(alarm_factory),
      clock_(clock),
      delegate_(delegate),
      config_(config),
      create_session_alarm_(QuicWrapUnique(
          alarm_factory_->CreateAlarm(new CreateSessionDelegate(this)))),
      factory_(QuicMakeUnique<QuartcFactory>(
          CreateFactoryConfig(alarm_factory, clock))) {
  DCHECK_EQ(config_.perspective, Perspective::IS_CLIENT);
  create_session_alarm_->Set(clock_->Now());
}

void QuartcAlarmEndpointImpl::OnCreateSessionAlarm() {
  session_ = factory_->CreateQuartcSession(config_);
  delegate_->OnSessionCreated(session_.get());
}

// Implementation of QuartcEndpoint which uses a QuartcDispatcher to listen for
// an incoming CHLO and create a session when one arrives.  Only suitable for
// use with the server perspective.
class QuartcDispatcherEndpointImpl : public QuartcEndpointImpl,
                                     public QuartcDispatcher::Delegate {
 public:
  QuartcDispatcherEndpointImpl(QuicAlarmFactory* alarm_factory,
                               const QuicClock* clock,
                               QuartcEndpoint::Delegate* delegate,
                               const QuartcSessionConfig& config);

  void OnSessionCreated(QuartcSession* session) override {
    delegate_->OnSessionCreated(session);
  }

 private:
  // Delegate which receives callbacks for newly created sessions.
  QuartcEndpoint::Delegate* delegate_;

  // Config to be used for new sessions.
  QuartcSessionConfig config_;

  // QuartcDispatcher waits for an incoming CHLO, then either rejects it or
  // creates a session to respond to it.  The dispatcher owns all sessions it
  // creates.
  std::unique_ptr<QuartcDispatcher> dispatcher_;
};

QuartcDispatcherEndpointImpl::QuartcDispatcherEndpointImpl(
    QuicAlarmFactory* alarm_factory,
    const QuicClock* clock,
    QuartcEndpoint::Delegate* delegate,
    const QuartcSessionConfig& config)
    : delegate_(delegate), config_(config) {
  DCHECK_EQ(config_.perspective, Perspective::IS_SERVER);
  auto connection_helper = QuicMakeUnique<QuartcConnectionHelper>(clock);
  auto crypto_config = CreateCryptoServerConfig(
      connection_helper->GetRandomGenerator(), clock, config.pre_shared_key);
  dispatcher_ = QuicMakeUnique<QuartcDispatcher>(
      QuicMakeUnique<QuicConfig>(CreateQuicConfig(config)),
      std::move(crypto_config),
      QuicMakeUnique<QuicVersionManager>(AllSupportedVersions()),
      std::move(connection_helper),
      QuicMakeUnique<QuartcCryptoServerStreamHelper>(),
      QuicMakeUnique<QuartcAlarmFactoryWrapper>(alarm_factory),
      QuicMakeUnique<QuartcPacketWriter>(config.packet_transport,
                                         config.max_packet_size),
      this);
  // The dispatcher requires at least one call to |ProcessBufferedChlos| to
  // set the number of connections it is allowed to create.
  dispatcher_->ProcessBufferedChlos(/*max_connections_to_create=*/1);
}

}  // namespace

QuartcEndpoint::QuartcEndpoint(QuicAlarmFactory* alarm_factory,
                               const QuicClock* clock,
                               Delegate* delegate)
    : alarm_factory_(alarm_factory), clock_(clock), delegate_(delegate) {}

void QuartcEndpoint::Connect(const QuartcSessionConfig& config) {
  if (config.perspective == Perspective::IS_CLIENT) {
    impl_ = QuicMakeUnique<QuartcAlarmEndpointImpl>(alarm_factory_, clock_,
                                                    delegate_, config);
  } else {
    impl_ = QuicMakeUnique<QuartcDispatcherEndpointImpl>(alarm_factory_, clock_,
                                                         delegate_, config);
  }
}

}  // namespace quic
