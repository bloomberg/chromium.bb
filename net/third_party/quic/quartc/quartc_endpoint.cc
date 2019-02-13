// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_endpoint.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"

namespace quic {

namespace {

QuartcFactoryConfig CreateFactoryConfig(QuicAlarmFactory* alarm_factory,
                                        const QuicClock* clock) {
  QuartcFactoryConfig config;
  config.alarm_factory = alarm_factory;
  config.clock = clock;
  return config;
}

}  // namespace

QuartcEndpoint::QuartcEndpoint(QuicAlarmFactory* alarm_factory,
                               const QuicClock* clock,
                               Delegate* delegate)
    : alarm_factory_(alarm_factory),
      clock_(clock),
      delegate_(delegate),
      create_session_alarm_(QuicWrapUnique(
          alarm_factory_->CreateAlarm(new CreateSessionDelegate(this)))),
      factory_(QuicMakeUnique<QuartcFactory>(
          CreateFactoryConfig(alarm_factory, clock))) {}

void QuartcEndpoint::Connect(const QuartcSessionConfig& config) {
  config_ = config;
  create_session_alarm_->Set(clock_->Now());
}

void QuartcEndpoint::OnCreateSessionAlarm() {
  session_ = factory_->CreateQuartcSession(config_);
  delegate_->OnSessionCreated(session_.get());
}

}  // namespace quic
