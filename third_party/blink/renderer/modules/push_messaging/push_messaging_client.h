// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-blink.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_subscription.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace mojom {
enum class PushRegistrationStatus;
}  // namespace mojom

class KURL;
struct Manifest;

class PushMessagingClient final
    : public GarbageCollectedFinalized<PushMessagingClient>,
      public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(PushMessagingClient);

 public:
  static const char kSupplementName[];

  explicit PushMessagingClient(LocalFrame& frame);
  ~PushMessagingClient() = default;

  static PushMessagingClient* From(LocalFrame* frame);

  void Subscribe(int64_t service_worker_registration_id,
                 const WebPushSubscriptionOptions& options,
                 bool user_gesture,
                 std::unique_ptr<WebPushSubscriptionCallbacks> callbacks);

 private:
  // Returns an initialized PushMessaging service. A connection will be
  // established after the first call to this method.
  mojom::blink::PushMessaging* GetService();

  void DidGetManifest(int64_t service_worker_registration_id,
                      mojom::blink::PushSubscriptionOptionsPtr options,
                      bool user_gesture,
                      std::unique_ptr<WebPushSubscriptionCallbacks> callbacks,
                      const WebURL& manifest_url,
                      const Manifest& manifest);

  void DoSubscribe(int64_t service_worker_registration_id,
                   mojom::blink::PushSubscriptionOptionsPtr options,
                   bool user_gesture,
                   std::unique_ptr<WebPushSubscriptionCallbacks> callbacks);

  void DidSubscribe(std::unique_ptr<WebPushSubscriptionCallbacks> callbacks,
                    mojom::blink::PushRegistrationStatus status,
                    const base::Optional<KURL>& endpoint,
                    mojom::blink::PushSubscriptionOptionsPtr options,
                    const base::Optional<WTF::Vector<uint8_t>>& p256dh,
                    const base::Optional<WTF::Vector<uint8_t>>& auth);

  mojom::blink::PushMessagingPtr push_messaging_manager_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingClient);
};

void ProvidePushMessagingClientTo(LocalFrame& frame,
                                  PushMessagingClient* client);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
