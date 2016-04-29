// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/telemetry_log_writer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "net/http/http_status_code.h"

namespace remoting {

const int kMaxTries = 5;

TelemetryLogWriter::TelemetryLogWriter(
    const std::string& telemetry_base_url,
    std::unique_ptr<UrlRequestFactory> request_factory)
    : telemetry_base_url_(telemetry_base_url),
      request_factory_(std::move(request_factory)) {}
TelemetryLogWriter::~TelemetryLogWriter() {}

void TelemetryLogWriter::SetAuthToken(const std::string& auth_token) {
  DCHECK(CalledOnValidThread());
  auth_token_ = auth_token;
  SendPendingEntries();
}

void TelemetryLogWriter::SetAuthClosure(base::Closure closure) {
  DCHECK(CalledOnValidThread());
  auth_closure_ = closure;
}

void TelemetryLogWriter::Log(const ChromotingEvent& entry) {
  DCHECK(CalledOnValidThread());
  pending_entries_.push_back(entry);
  SendPendingEntries();
}

void TelemetryLogWriter::SendPendingEntries() {
  DCHECK(CalledOnValidThread());
  if (request_ || pending_entries_.empty()) {
    return;
  }

  base::ListValue* events = new base::ListValue();
  while (!pending_entries_.empty()) {
    ChromotingEvent& entry = pending_entries_.front();
    events->Append(entry.CopyDictionaryValue());
    entry.IncrementTryCount();
    sending_entries_.push_back(std::move(entry));
    pending_entries_.pop_front();
  }
  base::DictionaryValue log_dictionary;
  log_dictionary.Set("event", std::unique_ptr<base::Value>(events));

  std::string json;
  JSONStringValueSerializer serializer(&json);
  if (!serializer.Serialize(log_dictionary)) {
    LOG(ERROR) << "Failed to serialize log to JSON.";
    return;
  }
  PostJsonToServer(json);
}

void TelemetryLogWriter::PostJsonToServer(const std::string& json) {
  DCHECK(CalledOnValidThread());
  DCHECK(!request_);
  request_ = request_factory_->CreateUrlRequest(UrlRequest::Type::POST,
                                                telemetry_base_url_);
  if (!auth_token_.empty()) {
    request_->AddHeader("Authorization:Bearer " + auth_token_);
  }

  VLOG(1) << "Posting log to telemetry server: " << json;

  request_->SetPostData("application/json", json);
  request_->Start(
      base::Bind(&TelemetryLogWriter::OnSendLogResult, base::Unretained(this)));
}

void TelemetryLogWriter::OnSendLogResult(
    const remoting::UrlRequest::Result& result) {
  DCHECK(CalledOnValidThread());
  DCHECK(request_);
  if (!result.success || result.status != net::HTTP_OK) {
    LOG(WARNING) << "Error occur when sending logs to the telemetry server, "
                 << "status: " << result.status;
    VLOG(1) << "Response body: \n"
            << "body: " << result.response_body;
  } else {
    VLOG(1) << "Successfully sent " << sending_entries_.size()
            << " log(s) to telemetry server.";
    sending_entries_.clear();
  }
  while (!sending_entries_.empty()) {
    ChromotingEvent& entry = sending_entries_.front();
    if (entry.try_count() < kMaxTries) {
      pending_entries_.push_back(std::move(entry));
    }
    sending_entries_.pop_front();
  }
  bool should_call_auth_closure =
      result.status == net::HTTP_UNAUTHORIZED && !auth_closure_.is_null();
  request_.reset();  // This may also destroy the result.
  if (should_call_auth_closure) {
    VLOG(1) << "Request is unauthorized. Trying to call the auth closure...";
    auth_closure_.Run();
  } else {
    SendPendingEntries();
  }
}

}  // namespace remoting
