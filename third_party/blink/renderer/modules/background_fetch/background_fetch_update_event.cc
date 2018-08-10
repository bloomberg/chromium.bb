// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_update_event.h"

#include "third_party/blink/public/platform/modules/background_fetch/web_background_fetch_settled_fetch.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_icon_loader.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_settled_fetch.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_ui_options.h"
#include "third_party/blink/renderer/modules/event_modules_names.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

BackgroundFetchUpdateEvent::BackgroundFetchUpdateEvent(
    const AtomicString& type,
    const BackgroundFetchSettledEventInit& initializer)
    : BackgroundFetchSettledEvent(type, initializer) {}

BackgroundFetchUpdateEvent::BackgroundFetchUpdateEvent(
    const AtomicString& type,
    const BackgroundFetchSettledEventInit& initializer,
    const String& unique_id,
    ScriptState* script_state,
    WaitUntilObserver* observer,
    ServiceWorkerRegistration* registration)
    : BackgroundFetchSettledEvent(type, initializer, unique_id, observer),
      registration_(registration),
      loader_(new BackgroundFetchIconLoader) {}

BackgroundFetchUpdateEvent::~BackgroundFetchUpdateEvent() = default;

void BackgroundFetchUpdateEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(loader_);
  BackgroundFetchSettledEvent::Trace(visitor);
}

ScriptPromise BackgroundFetchUpdateEvent::updateUI(
    ScriptState* script_state,
    const BackgroundFetchUIOptions& ui_options) {
  if (update_ui_called_) {
    // Return a rejected promise as this method should only be called once.
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "updateUI may only be called once."));
  }

  update_ui_called_ = true;

  if (!registration_) {
    // Return a Promise that will never settle when a developer calls this
    // method on a BackgroundFetchedEvent instance they created themselves.
    return ScriptPromise();
  }
  DCHECK(!unique_id_.IsEmpty());

  if (!ui_options.hasTitle() && ui_options.icons().IsEmpty()) {
    // Nothing to update, just return a resolved promise.
    ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (ui_options.icons().IsEmpty()) {
    DidGetIcon(resolver, ui_options.title(), SkBitmap());
  } else {
    loader_->Start(
        BackgroundFetchBridge::From(registration_),
        ExecutionContext::From(script_state), ui_options.icons(),
        WTF::Bind(&BackgroundFetchUpdateEvent::DidGetIcon, WrapPersistent(this),
                  WrapPersistent(resolver), ui_options.title()));
  }

  return promise;
}

void BackgroundFetchUpdateEvent::DidGetIcon(ScriptPromiseResolver* resolver,
                                            const String& title,
                                            const SkBitmap& icon) {
  BackgroundFetchBridge::From(registration_)
      ->UpdateUI(id(), unique_id_, title, icon,
                 WTF::Bind(&BackgroundFetchUpdateEvent::DidUpdateUI,
                           WrapPersistent(this), WrapPersistent(resolver)));
}

void BackgroundFetchUpdateEvent::DidUpdateUI(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
    case mojom::blink::BackgroundFetchError::INVALID_ID:
      resolver->Resolve();
      return;
    case mojom::blink::BackgroundFetchError::STORAGE_ERROR:
      resolver->Reject(
          DOMException::Create(DOMExceptionCode::kAbortError,
                               "Failed to update UI due to I/O error."));
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
    case mojom::blink::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

}  // namespace blink
