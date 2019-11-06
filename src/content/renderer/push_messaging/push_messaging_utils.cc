// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging/push_messaging_utils.h"

#include "content/public/common/service_names.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {

namespace {

const char* PushRegistrationStatusToString(
    blink::mojom::PushRegistrationStatus status) {
  switch (status) {
    case blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE:
    case blink::mojom::PushRegistrationStatus::
        SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE:
      return "Registration successful - from push service";

    case blink::mojom::PushRegistrationStatus::NO_SERVICE_WORKER:
      return "Registration failed - no Service Worker";

    case blink::mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE:
      return "Registration failed - push service not available";

    case blink::mojom::PushRegistrationStatus::LIMIT_REACHED:
      return "Registration failed - registration limit has been reached";

    case blink::mojom::PushRegistrationStatus::PERMISSION_DENIED:
      return "Registration failed - permission denied";

    case blink::mojom::PushRegistrationStatus::SERVICE_ERROR:
      return "Registration failed - push service error";

    case blink::mojom::PushRegistrationStatus::NO_SENDER_ID:
      return "Registration failed - missing applicationServerKey, and "
             "gcm_sender_id not found in manifest";

    case blink::mojom::PushRegistrationStatus::STORAGE_ERROR:
      return "Registration failed - storage error";

    case blink::mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE:
      return "Registration successful - from cache";

    case blink::mojom::PushRegistrationStatus::NETWORK_ERROR:
      return "Registration failed - could not connect to push server";

    case blink::mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED:
      // We split this out for UMA, but it must be indistinguishable to JS.
      return PushRegistrationStatusToString(
          blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);

    case blink::mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE:
      return "Registration failed - could not retrieve the public key";

    case blink::mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING:
      return "Registration failed - missing applicationServerKey, and manifest "
             "empty or missing";

    case blink::mojom::PushRegistrationStatus::SENDER_ID_MISMATCH:
      return "Registration failed - A subscription with a different "
             "applicationServerKey (or gcm_sender_id) already exists; to "
             "change the applicationServerKey, unsubscribe then resubscribe.";

    case blink::mojom::PushRegistrationStatus::STORAGE_CORRUPT:
      return "Registration failed - storage corrupt";

    case blink::mojom::PushRegistrationStatus::RENDERER_SHUTDOWN:
      return "Registration failed - renderer shutdown";
  }
  NOTREACHED();
  return "";
}

}  // namespace

blink::WebPushError PushRegistrationStatusToWebPushError(
    blink::mojom::PushRegistrationStatus status) {
  blink::WebPushError::ErrorType error_type =
      blink::WebPushError::kErrorTypeAbort;
  switch (status) {
    case blink::mojom::PushRegistrationStatus::PERMISSION_DENIED:
      error_type = blink::WebPushError::kErrorTypeNotAllowed;
      break;
    case blink::mojom::PushRegistrationStatus::SENDER_ID_MISMATCH:
      error_type = blink::WebPushError::kErrorTypeInvalidState;
      break;
    case blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE:
    case blink::mojom::PushRegistrationStatus::
        SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE:
    case blink::mojom::PushRegistrationStatus::NO_SERVICE_WORKER:
    case blink::mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE:
    case blink::mojom::PushRegistrationStatus::LIMIT_REACHED:
    case blink::mojom::PushRegistrationStatus::SERVICE_ERROR:
    case blink::mojom::PushRegistrationStatus::NO_SENDER_ID:
    case blink::mojom::PushRegistrationStatus::STORAGE_ERROR:
    case blink::mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE:
    case blink::mojom::PushRegistrationStatus::NETWORK_ERROR:
    case blink::mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED:
    case blink::mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE:
    case blink::mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING:
    case blink::mojom::PushRegistrationStatus::STORAGE_CORRUPT:
    case blink::mojom::PushRegistrationStatus::RENDERER_SHUTDOWN:
      error_type = blink::WebPushError::kErrorTypeAbort;
      break;
  }
  return blink::WebPushError(
      error_type,
      blink::WebString::FromUTF8(PushRegistrationStatusToString(status)));
}

}  // namespace content
