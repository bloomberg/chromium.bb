// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_

namespace blink {

namespace mojom {
enum class PushRegistrationStatus;
}  // namespace mojom

struct WebPushError;

WebPushError PushRegistrationStatusToWebPushError(
    mojom::PushRegistrationStatus status);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
