// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_TELEMETRY_LOGGER_H_
#define REMOTING_CLIENT_CLIENT_TELEMETRY_LOGGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/chromoting_event_log_writer.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/performance_tracker.h"

namespace remoting {

// ClientTelemetryLogger sends client log entries to the telemetry server.
// The logger should be run entirely on one single thread.
// TODO(yuweih): Implement new features that session_logger.js provides.
class ClientTelemetryLogger {
 public:
  explicit ClientTelemetryLogger(ChromotingEvent::Mode mode);
  ~ClientTelemetryLogger();

  // Start must be called before posting logs or setting auth token or closure.
  void Start(std::unique_ptr<UrlRequestFactory> request_factory,
             const std::string& telemetry_base_url);

  void LogSessionStateChange(ChromotingEvent::SessionState state,
                             ChromotingEvent::ConnectionError error);

  // TODO(yuweih): Investigate possibility of making PerformanceTracker const.
  void LogStatistics(protocol::PerformanceTracker* perf_tracker);

  // Authorization: The caller can either
  // 1. have its own fetching schedule and manually call |SetAuthToken| when the
  //    token is fetched or renewed
  // 2. or call |SetAuthClosure| and expect the logger to run the closure when
  //    it needs new token. See comments below.

  // Sets the auth token immediately.
  void SetAuthToken(const std::string& token);

  // Sets the authorization closure. The closure should call |SetAuthToken| to
  // set the token. The closure will be run when the logger needs authorization
  // to send out the logs.
  void SetAuthClosure(const base::Closure& closure);

  const std::string& session_id() const { return session_id_; }

  void SetSessionIdGenerationTimeForTest(base::TimeTicks gen_time);
  // Start the logger with a given log writer. Do not call Start before or
  // after calling this function.
  void StartForTest(std::unique_ptr<ChromotingEventLogWriter> writer);

  static ChromotingEvent::SessionState TranslateState(
      protocol::ConnectionToHost::State state);

  static ChromotingEvent::ConnectionError TranslateError(
      protocol::ErrorCode state);

 private:

  void FillEventContext(ChromotingEvent* event) const;

  // Generates a new random session ID.
  void GenerateSessionId();

  // Expire the session ID if the maximum duration has been exceeded.
  // Sends SessionIdOld and SessionIdNew events describing the change of id.
  void ExpireSessionIdIfOutdated();

  ChromotingEvent MakeStatsEvent(protocol::PerformanceTracker* perf_tracker);
  ChromotingEvent MakeSessionStateChangeEvent(
      ChromotingEvent::SessionState state,
      ChromotingEvent::ConnectionError error);
  ChromotingEvent MakeSessionIdOldEvent();
  ChromotingEvent MakeSessionIdNewEvent();

  // A randomly generated session ID to be attached to log messages. This
  // is regenerated at the start of a new session.
  std::string session_id_;

  base::TimeTicks session_start_time_;

  base::TimeTicks session_id_generation_time_;

  ChromotingEvent::Mode mode_;

  // The log writer that actually sends log to the server.
  std::unique_ptr<ChromotingEventLogWriter> log_writer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ClientTelemetryLogger);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_TELEMETRY_LOGGER_H_
