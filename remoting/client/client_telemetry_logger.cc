// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_telemetry_logger.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "remoting/base/telemetry_log_writer.h"

namespace {

const char kSessionIdAlphabet[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
const int kSessionIdLength = 20;

const int kMaxSessionIdAgeDays = 1;

}  // namespace

namespace remoting {

ClientTelemetryLogger::ClientTelemetryLogger(ChromotingEvent::Mode mode)
    : mode_(mode) {}

ClientTelemetryLogger::~ClientTelemetryLogger() {}

void ClientTelemetryLogger::Start(
    std::unique_ptr<UrlRequestFactory> request_factory,
    const std::string& telemetry_base_url) {
  DCHECK(!log_writer_);
  DCHECK(thread_checker_.CalledOnValidThread());
  log_writer_.reset(
      new TelemetryLogWriter(telemetry_base_url, std::move(request_factory)));
}

void ClientTelemetryLogger::LogSessionStateChange(
    ChromotingEvent::SessionState state,
    ChromotingEvent::ConnectionError error) {
  DCHECK(log_writer_) << "Please call Start before posting logs.";
  DCHECK(thread_checker_.CalledOnValidThread());
  ExpireSessionIdIfOutdated();

  if (session_id_.empty()) {
    GenerateSessionId();
  }
  if (session_start_time_.is_null()) {
    session_start_time_ = base::TimeTicks::Now();
  }

  ChromotingEvent event =
      ClientTelemetryLogger::MakeSessionStateChangeEvent(state, error);
  log_writer_->Log(event);

  if (ChromotingEvent::IsEndOfSession(state)) {
    session_id_.clear();
    session_start_time_ = base::TimeTicks();
  }
}

void ClientTelemetryLogger::LogStatistics(
    protocol::PerformanceTracker* perf_tracker) {
  DCHECK(log_writer_) << "Please call Start before posting logs.";
  DCHECK(thread_checker_.CalledOnValidThread());
  ExpireSessionIdIfOutdated();

  ChromotingEvent event = MakeStatsEvent(perf_tracker);
  log_writer_->Log(event);
}

void ClientTelemetryLogger::SetAuthToken(const std::string& token) {
  DCHECK(log_writer_) << "Please call Start before setting the token.";
  DCHECK(thread_checker_.CalledOnValidThread());
  log_writer_->SetAuthToken(token);
}

void ClientTelemetryLogger::SetAuthClosure(const base::Closure& closure) {
  DCHECK(log_writer_) << "Please call Start before setting the closure.";
  DCHECK(thread_checker_.CalledOnValidThread());
  log_writer_->SetAuthClosure(closure);
}

void ClientTelemetryLogger::SetSessionIdGenerationTimeForTest(
    base::TimeTicks gen_time) {
  session_id_generation_time_ = gen_time;
}

void ClientTelemetryLogger::StartForTest(
    std::unique_ptr<ChromotingEventLogWriter> writer) {
  DCHECK(!log_writer_);
  log_writer_ = std::move(writer);
}

// static
ChromotingEvent::SessionState ClientTelemetryLogger::TranslateState(
    protocol::ConnectionToHost::State state) {
  switch (state) {
    case protocol::ConnectionToHost::State::INITIALIZING:
      return ChromotingEvent::SessionState::INITIALIZING;
    case protocol::ConnectionToHost::State::CONNECTING:
      return ChromotingEvent::SessionState::CONNECTING;
    case protocol::ConnectionToHost::State::AUTHENTICATED:
      return ChromotingEvent::SessionState::AUTHENTICATED;
    case protocol::ConnectionToHost::State::CONNECTED:
      return ChromotingEvent::SessionState::CONNECTED;
    case protocol::ConnectionToHost::State::FAILED:
      return ChromotingEvent::SessionState::CONNECTION_FAILED;
    case protocol::ConnectionToHost::State::CLOSED:
      return ChromotingEvent::SessionState::CLOSED;
    default:
      NOTREACHED();
      return ChromotingEvent::SessionState::UNKNOWN;
  }
}

// static
ChromotingEvent::ConnectionError ClientTelemetryLogger::TranslateError(
    protocol::ErrorCode error) {
  switch (error) {
    case protocol::OK:
      return ChromotingEvent::ConnectionError::NONE;
    case protocol::PEER_IS_OFFLINE:
      return ChromotingEvent::ConnectionError::HOST_OFFLINE;
    case protocol::SESSION_REJECTED:
      return ChromotingEvent::ConnectionError::SESSION_REJECTED;
    case protocol::INCOMPATIBLE_PROTOCOL:
      return ChromotingEvent::ConnectionError::INCOMPATIBLE_PROTOCOL;
    case protocol::AUTHENTICATION_FAILED:
      return ChromotingEvent::ConnectionError::AUTHENTICATION_FAILED;
    case protocol::INVALID_ACCOUNT:
      return ChromotingEvent::ConnectionError::INVALID_ACCOUNT;
    case protocol::CHANNEL_CONNECTION_ERROR:
      return ChromotingEvent::ConnectionError::P2P_FAILURE;
    case protocol::SIGNALING_ERROR:
      return ChromotingEvent::ConnectionError::NETWORK_FAILURE;
    case protocol::SIGNALING_TIMEOUT:
      return ChromotingEvent::ConnectionError::NETWORK_FAILURE;
    case protocol::HOST_OVERLOAD:
      return ChromotingEvent::ConnectionError::HOST_OVERLOAD;
    case protocol::MAX_SESSION_LENGTH:
      return ChromotingEvent::ConnectionError::MAX_SESSION_LENGTH;
    case protocol::HOST_CONFIGURATION_ERROR:
      return ChromotingEvent::ConnectionError::HOST_CONFIGURATION_ERROR;
    case protocol::UNKNOWN_ERROR:
      return ChromotingEvent::ConnectionError::UNKNOWN_ERROR;
    default:
      NOTREACHED();
      return ChromotingEvent::ConnectionError::UNEXPECTED;
  }
}

void ClientTelemetryLogger::FillEventContext(ChromotingEvent* event) const {
  event->SetEnum(ChromotingEvent::kModeKey, mode_);
  event->SetEnum(ChromotingEvent::kRoleKey, ChromotingEvent::Role::CLIENT);
  event->AddSystemInfo();
  if (!session_id_.empty()) {
    event->SetString(ChromotingEvent::kSessionIdKey, session_id_);
  }
  if (!session_start_time_.is_null()) {
    int session_duration =
        (base::TimeTicks::Now() - session_start_time_).InSeconds();
    event->SetInteger(ChromotingEvent::kSessionDurationKey, session_duration);
  }
}

void ClientTelemetryLogger::GenerateSessionId() {
  session_id_.resize(kSessionIdLength);
  for (int i = 0; i < kSessionIdLength; i++) {
    const int alphabet_size = arraysize(kSessionIdAlphabet) - 1;
    session_id_[i] = kSessionIdAlphabet[base::RandGenerator(alphabet_size)];
  }
  session_id_generation_time_ = base::TimeTicks::Now();
}

void ClientTelemetryLogger::ExpireSessionIdIfOutdated() {
  if (session_id_.empty()) {
    return;
  }

  base::TimeDelta max_age = base::TimeDelta::FromDays(kMaxSessionIdAgeDays);
  if (base::TimeTicks::Now() - session_id_generation_time_ > max_age) {
    // Log the old session ID.
    ChromotingEvent event = MakeSessionIdOldEvent();
    log_writer_->Log(event);

    // Generate a new session ID.
    GenerateSessionId();

    // Log the new session ID.
    ChromotingEvent new_id_event = MakeSessionIdNewEvent();
    log_writer_->Log(new_id_event);
  }
}

ChromotingEvent ClientTelemetryLogger::MakeStatsEvent(
    protocol::PerformanceTracker* perf_tracker) {
  ChromotingEvent event(ChromotingEvent::Type::CONNECTION_STATISTICS);
  FillEventContext(&event);

  event.SetDouble(ChromotingEvent::kVideoBandwidthKey,
                  perf_tracker->video_bandwidth());
  event.SetDouble(ChromotingEvent::kCaptureLatencyKey,
                  perf_tracker->video_capture_ms().Average());
  event.SetDouble(ChromotingEvent::kEncodeLatencyKey,
                  perf_tracker->video_encode_ms().Average());
  event.SetDouble(ChromotingEvent::kDecodeLatencyKey,
                  perf_tracker->video_decode_ms().Average());
  event.SetDouble(ChromotingEvent::kRenderLatencyKey,
                  perf_tracker->video_paint_ms().Average());
  event.SetDouble(ChromotingEvent::kRoundtripLatencyKey,
                  perf_tracker->round_trip_ms().Average());
  event.SetDouble(ChromotingEvent::kMaxCaptureLatencyKey,
                  perf_tracker->video_capture_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxEncodeLatencyKey,
                  perf_tracker->video_encode_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxDecodeLatencyKey,
                  perf_tracker->video_decode_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxRenderLatencyKey,
                  perf_tracker->video_paint_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxRoundtripLatencyKey,
                  perf_tracker->round_trip_ms().Max());

  return event;
}

ChromotingEvent ClientTelemetryLogger::MakeSessionStateChangeEvent(
    ChromotingEvent::SessionState state,
    ChromotingEvent::ConnectionError error) {
  ChromotingEvent event(ChromotingEvent::Type::SESSION_STATE);
  FillEventContext(&event);
  event.SetEnum(ChromotingEvent::kSessionStateKey, state);
  event.SetEnum(ChromotingEvent::kConnectionErrorKey, error);
  return event;
}

ChromotingEvent ClientTelemetryLogger::MakeSessionIdOldEvent() {
  ChromotingEvent event(ChromotingEvent::Type::SESSION_ID_OLD);
  FillEventContext(&event);
  return event;
}

ChromotingEvent ClientTelemetryLogger::MakeSessionIdNewEvent() {
  ChromotingEvent event(ChromotingEvent::Type::SESSION_ID_NEW);
  FillEventContext(&event);
  return event;
}

}  // namespace remoting
