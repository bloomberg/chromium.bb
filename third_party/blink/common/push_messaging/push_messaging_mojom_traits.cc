// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/push_messaging/push_messaging_mojom_traits.h"

#include <string>

namespace mojo {

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

}  // namespace mojo
