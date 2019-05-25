// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_provider.h"

#include <utility>

#include "third_party/blink/public/common/push_messaging/web_push_subscription_options.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-shared.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_error.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_type_converters.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

WebPushError::ErrorType PushErrorTypeToWebPushErrorType(
    mojom::PushErrorType error_type) {
  WebPushError::ErrorType web_push_error_type;
  switch (error_type) {
    case mojom::PushErrorType::ABORT:
      web_push_error_type = WebPushError::ErrorType::kErrorTypeAbort;
      break;
    case mojom::PushErrorType::NETWORK:
      web_push_error_type = WebPushError::ErrorType::kErrorTypeNetwork;
      break;
    case mojom::PushErrorType::NONE:
      web_push_error_type = WebPushError::ErrorType::kErrorTypeNone;
      break;
    case mojom::PushErrorType::NOT_ALLOWED:
      web_push_error_type = WebPushError::ErrorType::kErrorTypeNotAllowed;
      break;
    case mojom::PushErrorType::NOT_FOUND:
      web_push_error_type = WebPushError::ErrorType::kErrorTypeNotFound;
      break;
    case mojom::PushErrorType::NOT_SUPPORTED:
      web_push_error_type = WebPushError::ErrorType::kErrorTypeNotSupported;
      break;
    case mojom::PushErrorType::INVALID_STATE:
      web_push_error_type = WebPushError::ErrorType::kErrorTypeInvalidState;
      break;
  }

  return web_push_error_type;
}

}  // namespace

// static
const char PushProvider::kSupplementName[] = "PushProvider";

PushProvider::PushProvider(ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration) {
  GetInterface(mojo::MakeRequest(&push_messaging_manager_));
}

// static
PushProvider* PushProvider::From(ServiceWorkerRegistration* registration) {
  DCHECK(registration);

  PushProvider* provider =
      Supplement<ServiceWorkerRegistration>::From<PushProvider>(registration);

  if (!provider) {
    provider = MakeGarbageCollected<PushProvider>(*registration);
    ProvideTo(*registration, provider);
  }

  return provider;
}

// static
void PushProvider::GetInterface(mojom::blink::PushMessagingRequest request) {
  Platform::Current()->GetInterfaceProvider()->GetInterface(std::move(request));
}

void PushProvider::Subscribe(
    const WebPushSubscriptionOptions& options,
    bool user_gesture,
    std::unique_ptr<WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  mojom::blink::PushSubscriptionOptionsPtr content_options_ptr =
      mojom::blink::PushSubscriptionOptions::From(&options);

  push_messaging_manager_->Subscribe(
      -1 /* Invalid ID */, GetSupplementable()->RegistrationId(),
      std::move(content_options_ptr), user_gesture,
      WTF::Bind(&PushProvider::DidSubscribe, WrapPersistent(this),
                WTF::Passed(std::move(callbacks))));
}

void PushProvider::DidSubscribe(
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

    WebPushSubscriptionOptions content_options =
        mojo::ConvertTo<WebPushSubscriptionOptions>(std::move(options));
    callbacks->OnSuccess(std::make_unique<WebPushSubscription>(
        endpoint.value(), content_options.user_visible_only,
        WebString::FromLatin1(content_options.application_server_key),
        p256dh.value(), auth.value()));
  } else {
    callbacks->OnError(PushRegistrationStatusToWebPushError(status));
  }
}

void PushProvider::Unsubscribe(
    std::unique_ptr<WebPushUnsubscribeCallbacks> callbacks) {
  DCHECK(callbacks);

  push_messaging_manager_->Unsubscribe(
      GetSupplementable()->RegistrationId(),
      WTF::Bind(&PushProvider::DidUnsubscribe, WrapPersistent(this),
                WTF::Passed(std::move(callbacks))));
}

void PushProvider::DidUnsubscribe(
    std::unique_ptr<WebPushUnsubscribeCallbacks> callbacks,
    mojom::blink::PushErrorType error_type,
    bool did_unsubscribe,
    const WTF::String& error_message) {
  DCHECK(callbacks);

  // ErrorTypeNone indicates success.
  if (error_type == mojom::blink::PushErrorType::NONE) {
    callbacks->OnSuccess(did_unsubscribe);
  } else {
    callbacks->OnError(WebPushError(PushErrorTypeToWebPushErrorType(error_type),
                                    WebString(error_message)));
  }
}

void PushProvider::GetSubscription(
    std::unique_ptr<WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  push_messaging_manager_->GetSubscription(
      GetSupplementable()->RegistrationId(),
      WTF::Bind(&PushProvider::DidGetSubscription, WrapPersistent(this),
                WTF::Passed(std::move(callbacks))));
}

void PushProvider::DidGetSubscription(
    std::unique_ptr<WebPushSubscriptionCallbacks> callbacks,
    mojom::blink::PushGetRegistrationStatus status,
    const base::Optional<KURL>& endpoint,
    mojom::blink::PushSubscriptionOptionsPtr options,
    const base::Optional<WTF::Vector<uint8_t>>& p256dh,
    const base::Optional<WTF::Vector<uint8_t>>& auth) {
  DCHECK(callbacks);

  if (status == mojom::blink::PushGetRegistrationStatus::SUCCESS) {
    DCHECK(endpoint);
    DCHECK(options);
    DCHECK(p256dh);
    DCHECK(auth);

    WebPushSubscriptionOptions content_options =
        mojo::ConvertTo<WebPushSubscriptionOptions>(std::move(options));
    callbacks->OnSuccess(std::make_unique<WebPushSubscription>(
        endpoint.value(), content_options.user_visible_only,
        WebString::FromLatin1(content_options.application_server_key),
        p256dh.value(), auth.value()));
  } else {
    // We are only expecting an error if we can't find a registration.
    callbacks->OnSuccess(nullptr);
  }
}

}  // namespace blink
