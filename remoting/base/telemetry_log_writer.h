// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
#define REMOTING_BASE_TELEMETRY_LOG_WRITER_H_

#include <deque>
#include <string>

#include "base/callback.h"
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
class TelemetryLogWriter : public ChromotingEventLogWriter {
 public:
  TelemetryLogWriter(const std::string& telemetry_base_url,
                     std::unique_ptr<UrlRequestFactory> request_factory);

  // "Authorization:Bearer {TOKEN}" will be added if auth_token is not empty.
  // After this function is called, the log writer will try to send out pending
  // logs if the list is not empty.
  void SetAuthToken(const std::string& auth_token) override;

  // The closure will be called when the request fails with unauthorized error
  // code. The closure should call SetAuthToken to set the token.
  // If the closure is not set, the log writer will try to resend the logs
  // immediately.
  void SetAuthClosure(const base::Closure& closure) override;

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
  std::string auth_token_;
  base::Closure auth_closure_;
  std::unique_ptr<UrlRequest> request_;

  // Entries to be sent.
  std::deque<ChromotingEvent> pending_entries_;

  // Entries being sent.
  // These will be pushed back to pending_entries if error occurs.
  std::deque<ChromotingEvent> sending_entries_;

  DISALLOW_COPY_AND_ASSIGN(TelemetryLogWriter);
};

}  // namespace remoting
#endif  // REMOTING_BASE_TELEMETRY_LOG_WRITER_H_
