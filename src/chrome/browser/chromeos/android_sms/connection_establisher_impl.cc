// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_establisher_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace chromeos {

namespace android_sms {

const char ConnectionEstablisherImpl::kStartStreamingMessage[] =
    "start_streaming_connection";

const char ConnectionEstablisherImpl::kResumeStreamingMessage[] =
    "resume_streaming_connection";

ConnectionEstablisherImpl::ConnectionEstablisherImpl(base::Clock* clock)
    : clock_(clock) {}
ConnectionEstablisherImpl::~ConnectionEstablisherImpl() = default;

void ConnectionEstablisherImpl::EstablishConnection(
    content::ServiceWorkerContext* service_worker_context,
    ConnectionMode connection_mode) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &ConnectionEstablisherImpl::SendStartStreamingMessageIfNotConnected,
          base::Unretained(this), service_worker_context, connection_mode));
}

void ConnectionEstablisherImpl::SendStartStreamingMessageIfNotConnected(
    content::ServiceWorkerContext* service_worker_context,
    ConnectionMode connection_mode) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (is_connected_) {
    PA_LOG(VERBOSE) << "Connection already exists. Skipped sending start "
                       "streaming message to service worker.";
    return;
  }

  blink::TransferableMessage msg;
  switch (connection_mode) {
    case ConnectionMode::kStartConnection:
      msg.owned_encoded_message =
          blink::EncodeStringMessage(base::UTF8ToUTF16(kStartStreamingMessage));
      break;
    case ConnectionMode::kResumeExistingConnection:
      msg.owned_encoded_message = blink::EncodeStringMessage(
          base::UTF8ToUTF16(kResumeStreamingMessage));
      break;
  }
  msg.encoded_message = msg.owned_encoded_message;

  PA_LOG(VERBOSE) << "Dispatching start streaming message to service worker.";
  is_connected_ = true;
  start_connection_message_time_ = clock_->Now();
  service_worker_context->StartServiceWorkerAndDispatchLongRunningMessage(
      GetAndroidMessagesURL(), std::move(msg),
      base::BindOnce(&ConnectionEstablisherImpl::OnMessageDispatchResult,
                     base::Unretained(this)));
}

void ConnectionEstablisherImpl::OnMessageDispatchResult(bool status) {
  // When message dispatch result callback is called, it means that the service
  // worker resolved it's message handler promise and is not holding a
  // background connection.
  PA_LOG(VERBOSE)
      << "Service worker streaming message dispatch returned status: "
      << status;
  is_connected_ = false;

  // |status| indicates the success/failure for the dispatch of the message. If
  // |status| is false then the message was never successfully dispatched.
  // If |status| is true, then the message was dispatched and resolved by the
  // service worker successfully.
  UMA_HISTOGRAM_BOOLEAN("AndroidSms.ServiceWorkerMessageDispatchStatus",
                        status);
  if (status) {
    // The service worker could have received the message but failed and
    // resolved early. Track these failures by measuring service worker
    // lifetime.
    UMA_HISTOGRAM_CUSTOM_TIMES("AndroidSms.ServiceWorkerLifetime",
                               clock_->Now() - start_connection_message_time_,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromHours(1), 50);
  }
}

}  // namespace android_sms

}  // namespace chromeos
