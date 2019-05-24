// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/push_messaging/push_messaging_mojom_traits.h"

#include <string>

namespace mojo {

// PushErrorType
static_assert(blink::WebPushError::ErrorType::kErrorTypeAbort ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::ABORT),
              "PushErrorType enums must match, ABORT");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNetwork ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::NETWORK),
              "PushErrorType enums must match, NETWORK");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNone ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::NONE),
              "PushErrorType enums must match, NONE");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNotAllowed ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::NOT_ALLOWED),
              "PushErrorType enums must match, NOT_ALLOWED");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNotFound ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::NOT_FOUND),
              "PushErrorType enums must match, NOT_FOUND");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNotSupported ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::NOT_SUPPORTED),
              "PushErrorType enums must match, NOT_SUPPORTED");

static_assert(blink::WebPushError::ErrorType::kErrorTypeInvalidState ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::INVALID_STATE),
              "PushErrorType enums must match, INVALID_STATE");

static_assert(blink::WebPushError::ErrorType::kErrorTypeLast ==
                  static_cast<blink::WebPushError::ErrorType>(
                      blink::mojom::PushErrorType::kMaxValue),
              "PushErrorType enums must match, kMaxValue");

// static
bool StructTraits<blink::mojom::PushSubscriptionOptionsDataView,
                  blink::WebPushSubscriptionOptions>::
    Read(blink::mojom::PushSubscriptionOptionsDataView data,
         blink::WebPushSubscriptionOptions* out) {
  out->user_visible_only = data.user_visible_only();

  std::vector<uint8_t> application_server_key_vector;
  if (!data.ReadApplicationServerKey(&application_server_key_vector)) {
    return false;
  }

  out->application_server_key =
      std::string(application_server_key_vector.begin(),
                  application_server_key_vector.end());
  return true;
}

// static
blink::mojom::PushErrorType
EnumTraits<blink::mojom::PushErrorType, blink::WebPushError::ErrorType>::
    ToMojom(blink::WebPushError::ErrorType input) {
  if (input >= blink::WebPushError::ErrorType::kErrorTypeAbort &&
      input <= blink::WebPushError::ErrorType::kErrorTypeInvalidState) {
    return static_cast<blink::mojom::PushErrorType>(input);
  }

  NOTREACHED();
  return blink::mojom::PushErrorType::ABORT;
}

// static
bool EnumTraits<blink::mojom::PushErrorType, blink::WebPushError::ErrorType>::
    FromMojom(blink::mojom::PushErrorType input,
              blink::WebPushError::ErrorType* output) {
  if (input >= blink::mojom::PushErrorType::ABORT &&
      input <= blink::mojom::PushErrorType::INVALID_STATE) {
    *output = static_cast<blink::WebPushError::ErrorType>(input);
    return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
