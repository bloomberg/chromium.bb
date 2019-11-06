// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_

#include "third_party/blink/public/platform/modules/push_messaging/web_push_error.h"

namespace blink {

namespace mojom {
enum class PushRegistrationStatus;
}  // namespace mojom

}  // namespace blink

namespace content {

blink::WebPushError PushRegistrationStatusToWebPushError(
    blink::mojom::PushRegistrationStatus status);

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
