// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PUSH_MESSAGING_PUSH_MESSAGING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PUSH_MESSAGING_PUSH_MESSAGING_MOJOM_TRAITS_H_

#include <stddef.h>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::PushSubscriptionOptionsDataView,
                 blink::PushSubscriptionOptionsParams> {
  static bool user_visible_only(const blink::PushSubscriptionOptionsParams& r) {
    return r.user_visible_only;
  }
  static const std::string& sender_info(
      const blink::PushSubscriptionOptionsParams& r) {
    return r.sender_info;
  }
  static bool Read(blink::mojom::PushSubscriptionOptionsDataView data,
                   blink::PushSubscriptionOptionsParams* out);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::PushErrorType, blink::WebPushError::ErrorType> {
  static blink::mojom::PushErrorType ToMojom(
      blink::WebPushError::ErrorType input);
  static bool FromMojom(blink::mojom::PushErrorType input,
                        blink::WebPushError::ErrorType* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PUSH_MESSAGING_PUSH_MESSAGING_MOJOM_TRAITS_H_
