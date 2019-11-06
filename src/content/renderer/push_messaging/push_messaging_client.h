// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_client.h"

class GURL;

namespace blink {

namespace mojom {
enum class PushRegistrationStatus;
}

struct Manifest;
struct WebPushSubscriptionOptions;
struct WebPushSubscriptionOptions;
}  // namespace blink

namespace content {

class PushMessagingClient : public RenderFrameObserver,
                            public blink::WebPushClient {
 public:
  explicit PushMessagingClient(RenderFrame* render_frame);
  ~PushMessagingClient() override;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // WebPushClient implementation.
  void Subscribe(
      int64_t service_worker_registration_id,
      const blink::WebPushSubscriptionOptions& options,
      bool user_gesture,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) override;

  void DidGetManifest(
      int64_t service_worker_registration_id,
      const blink::WebPushSubscriptionOptions& options,
      bool user_gesture,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
      const blink::WebURL& manifest_url,
      const blink::Manifest& manifest);

  void DoSubscribe(
      int64_t service_worker_registration_id,
      const blink::WebPushSubscriptionOptions& options,
      bool user_gesture,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks);

  void DidSubscribe(
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
      blink::mojom::PushRegistrationStatus status,
      const base::Optional<GURL>& endpoint,
      const base::Optional<blink::WebPushSubscriptionOptions>& options,
      const base::Optional<std::vector<uint8_t>>& p256dh,
      const base::Optional<std::vector<uint8_t>>& auth);

  blink::mojom::PushMessagingPtr push_messaging_manager_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
