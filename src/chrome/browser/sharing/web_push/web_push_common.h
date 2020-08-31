// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEB_PUSH_WEB_PUSH_COMMON_H_
#define CHROME_BROWSER_SHARING_WEB_PUSH_WEB_PUSH_COMMON_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"

// Message to be delivered to the other party via Web Push.
struct WebPushMessage {
  WebPushMessage();
  WebPushMessage(WebPushMessage&& other);
  ~WebPushMessage();
  WebPushMessage& operator=(WebPushMessage&& other);

  // Urgency of a WebPushMessage as defined in RFC 8030 section 5.3.
  // https://tools.ietf.org/html/rfc8030#section-5.3
  enum class Urgency {
    kVeryLow,
    kLow,
    kNormal,
    kHigh,
  };

  // In seconds.
  int time_to_live = kMaximumTTL;
  std::string payload;
  Urgency urgency = Urgency::kNormal;

  static const int kMaximumTTL;

  WebPushMessage(const WebPushMessage&) = delete;
  WebPushMessage& operator=(const WebPushMessage&) = delete;
};

// Result of sending web push message.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendWebPushMessageResult {
  kSuccessful = 0,
  kEncryptionFailed = 1,
  kCreateJWTFailed = 2,
  kNetworkError = 3,
  kServerError = 4,
  kParseResponseFailed = 5,
  kVapidKeyInvalid = 6,
  kDeviceGone = 7,
  kPayloadTooLarge = 8,
  kMaxValue = kPayloadTooLarge,
};

using WebPushCallback = base::OnceCallback<void(SendWebPushMessageResult,
                                                base::Optional<std::string>)>;

// Invoke |callback| with |result| and logs the |result| to UMA. This should be
// called when after a web push message is sent. If |result| is
// SendWebPushMessageResult::Successful, |message_id| must be present.
void InvokeWebPushCallback(
    WebPushCallback callback,
    SendWebPushMessageResult result,
    base::Optional<std::string> message_id = base::nullopt);

// Logs the size of message payload to UMA. This should be called right before a
// web push message is sent.
void LogSendWebPushMessagePayloadSize(int size);

// Logs the network error or status code after a web push message is sent.
void LogSendWebPushMessageStatusCode(int status_code);

// Categorize response body when 403: Forbidden is received and log as enum.
void LogSendWebPushMessageForbiddenBody(const std::string* response_body);

#endif  // CHROME_BROWSER_SHARING_WEB_PUSH_WEB_PUSH_COMMON_H_
