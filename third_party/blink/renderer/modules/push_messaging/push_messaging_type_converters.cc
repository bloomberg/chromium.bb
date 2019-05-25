// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_messaging_type_converters.h"

#include <string>
#include <utility>

#include "third_party/blink/public/common/push_messaging/web_push_subscription_options.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

blink::WebPushSubscriptionOptions
TypeConverter<blink::WebPushSubscriptionOptions,
              blink::mojom::blink::PushSubscriptionOptionsPtr>::
    Convert(const blink::mojom::blink::PushSubscriptionOptionsPtr& input) {
  blink::WebPushSubscriptionOptions options;
  options.user_visible_only = input->user_visible_only;
  options.application_server_key =
      std::string(input->application_server_key.begin(),
                  input->application_server_key.end());
  return options;
}

blink::mojom::blink::PushSubscriptionOptionsPtr
TypeConverter<blink::mojom::blink::PushSubscriptionOptionsPtr,
              const blink::WebPushSubscriptionOptions*>::
    Convert(const blink::WebPushSubscriptionOptions* input) {
  auto output = blink::mojom::blink::PushSubscriptionOptions::New();
  output->user_visible_only = input->user_visible_only;
  output->application_server_key = WTF::Vector<uint8_t>();
  output->application_server_key.AppendRange(
      input->application_server_key.begin(),
      input->application_server_key.end());

  return output;
}

}  // namespace mojo
