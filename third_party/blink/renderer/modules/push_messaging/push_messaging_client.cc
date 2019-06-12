// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_messaging_client.h"

#include <string>
#include <utility>

#include "third_party/blink/public/common/push_messaging/web_push_subscription_options.h"
#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_type_converters.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

// static
const char PushMessagingClient::kSupplementName[] = "PushMessagingClient";

PushMessagingClient::PushMessagingClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {
  // This class will be instantiated for every page load (rather than on push
  // messaging use), so there's nothing to be done in this constructor.
}

// static
PushMessagingClient* PushMessagingClient::From(LocalFrame* frame) {
  DCHECK(frame);
  return Supplement<LocalFrame>::From<PushMessagingClient>(frame);
}

mojom::blink::PushMessaging* PushMessagingClient::GetService() {
  if (!push_messaging_manager_) {
    auto request = mojo::MakeRequest(
        &push_messaging_manager_,
        GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI));

    GetSupplementable()->GetDocumentInterfaceBroker().GetPushMessaging(
        std::move(request));
  }

  return push_messaging_manager_.get();
}

void PushMessagingClient::Subscribe(
    int64_t service_worker_registration_id,
    const WebPushSubscriptionOptions& options,
    bool user_gesture,
    std::unique_ptr<WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  mojom::blink::PushSubscriptionOptionsPtr options_ptr =
      mojom::blink::PushSubscriptionOptions::From(&options);

  // If a developer provided an application server key in |options|, skip
  // fetching the manifest.
  if (options.application_server_key.empty()) {
    ManifestManager* manifest_manager =
        ManifestManager::From(*GetSupplementable());
    manifest_manager->RequestManifest(
        WTF::Bind(&PushMessagingClient::DidGetManifest, WrapPersistent(this),
                  service_worker_registration_id, std::move(options_ptr),
                  user_gesture, std::move(callbacks)));
  } else {
    DoSubscribe(service_worker_registration_id, std::move(options_ptr),
                user_gesture, std::move(callbacks));
  }
}

void PushMessagingClient::DidGetManifest(
    int64_t service_worker_registration_id,
    mojom::blink::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    std::unique_ptr<WebPushSubscriptionCallbacks> callbacks,
    const KURL& manifest_url,
    mojom::blink::ManifestPtr manifest) {
  // Get the application_server_key from the manifest since it wasn't provided
  // by the caller.
  if (manifest == mojom::blink::Manifest::New()) {
    DidSubscribe(
        std::move(callbacks),
        mojom::blink::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING,
        base::nullopt, nullptr, base::nullopt, base::nullopt);
    return;
  }

  if (!manifest->gcm_sender_id.IsNull()) {
    StringUTF8Adaptor gcm_sender_id_as_utf8_string(manifest->gcm_sender_id);
    Vector<uint8_t> application_server_key;
    application_server_key.Append(gcm_sender_id_as_utf8_string.data(),
                                  gcm_sender_id_as_utf8_string.size());
    options->application_server_key = std::move(application_server_key);
  }

  DoSubscribe(service_worker_registration_id, std::move(options), user_gesture,
              std::move(callbacks));
}

void PushMessagingClient::DoSubscribe(
    int64_t service_worker_registration_id,
    mojom::blink::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    std::unique_ptr<WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  if (options->application_server_key.IsEmpty()) {
    DidSubscribe(std::move(callbacks),
                 mojom::blink::PushRegistrationStatus::NO_SENDER_ID,
                 base::nullopt, nullptr, base::nullopt, base::nullopt);
    return;
  }

  GetService()->Subscribe(
      service_worker_registration_id, std::move(options), user_gesture,
      WTF::Bind(&PushMessagingClient::DidSubscribe, WrapPersistent(this),
                std::move(callbacks)));
}

void PushMessagingClient::DidSubscribe(
    std::unique_ptr<WebPushSubscriptionCallbacks> callbacks,
    mojom::blink::PushRegistrationStatus status,
    const base::Optional<KURL>& endpoint,
    mojom::blink::PushSubscriptionOptionsPtr options,
    const base::Optional<WTF::Vector<uint8_t>>& p256dh,
    const base::Optional<WTF::Vector<uint8_t>>& auth) {
  DCHECK(callbacks);

  if (status ==
          mojom::blink::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::
                    SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::SUCCESS_FROM_CACHE) {
    DCHECK(endpoint);
    DCHECK(options);
    DCHECK(p256dh);
    DCHECK(auth);

    WebPushSubscriptionOptions web_options =
        mojo::ConvertTo<WebPushSubscriptionOptions>(std::move(options));
    callbacks->OnSuccess(std::make_unique<WebPushSubscription>(
        endpoint.value(), web_options.user_visible_only,
        WebString::FromLatin1(web_options.application_server_key),
        p256dh.value(), auth.value()));
  } else {
    callbacks->OnError(PushRegistrationStatusToWebPushError(status));
  }
}

// static
void ProvidePushMessagingClientTo(LocalFrame& frame,
                                  PushMessagingClient* client) {
  PushMessagingClient::ProvideTo(frame, client);
}

}  // namespace blink
