// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
#define REMOTING_BASE_TELEMETRY_LOG_WRITER_H_

#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/chromoting_event_log_writer.h"
#include "remoting/base/url_request.h"

namespace remoting {

// TelemetryLogWriter sends log entries (ChromotingEvent) to the telemetry
// server.
// Logs to be sent will be queued and sent when it is available. Logs failed
// to send will be retried for a few times and dropped if they still can't be
// sent.
// The log writer should be used entirely on one thread after it is created,
// unless otherwise noted.
class TelemetryLogWriter : public ChromotingEventLogWriter {
 public:
  // |auth_closure| will be called when the request fails with unauthorized
  // error code. The closure should call SetAccessToken to set the token.
  // If the closure is null, the log writer will try to resend the logs
  // immediately.
  // TODO(yuweih): Pass in centralized OAuthTokenGetter.
  TelemetryLogWriter(const std::string& telemetry_base_url,
                     std::unique_ptr<UrlRequestFactory> request_factory,
                     const base::RepeatingClosure& auth_closure);

  // "Authorization:Bearer {TOKEN}" will be added if access_token is not empty.
  // After this function is called, the log writer will try to send out pending
  // logs if the list is not empty.
  void SetAuthToken(const std::string& access_token) override;

  // Push the log entry to the pending list and send out all the pending logs.
  void Log(const ChromotingEvent& entry) override;

  ~TelemetryLogWriter() override;

 private:
  void SendPendingEntries();
  void PostJsonToServer(const std::string& json);
  void OnSendLogResult(const remoting::UrlRequest::Result& result);

  base::ThreadChecker thread_checker_;
  std::string telemetry_base_url_;
  std::unique_ptr<UrlRequestFactory> request_factory_;
  std::string access_token_;
  base::RepeatingClosure auth_closure_;
  std::unique_ptr<UrlRequest> request_;

  // Entries to be sent.
  base::circular_deque<ChromotingEvent> pending_entries_;

  // Entries being sent.
  // These will be pushed back to pending_entries if error occurs.
  base::circular_deque<ChromotingEvent> sending_entries_;

  DISALLOW_COPY_AND_ASSIGN(TelemetryLogWriter);
};

}  // namespace remoting
#endif  // REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
