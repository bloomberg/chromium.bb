// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webshare/NavigatorShare.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/webshare/ShareData.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

namespace {

// Gets the human-friendly error message for a ShareError. |error| must not be
// ShareError::OK.
String ErrorToString(mojom::blink::ShareError error) {
  switch (error) {
    case mojom::blink::ShareError::OK:
      NOTREACHED();
      break;
    case mojom::blink::ShareError::INTERNAL_ERROR:
      return "Share failed";
    case mojom::blink::ShareError::CANCELED:
      return "Share canceled";
  }
  NOTREACHED();
  return String();
}

}  // namespace

class NavigatorShare::ShareClientImpl final
    : public GarbageCollected<ShareClientImpl> {
 public:
  ShareClientImpl(NavigatorShare*, ScriptPromiseResolver*);

  void Callback(mojom::blink::ShareError);

  void OnConnectionError();

  DEFINE_INLINE_TRACE() {
    visitor->Trace(navigator_);
    visitor->Trace(resolver_);
  }

 private:
  WeakMember<NavigatorShare> navigator_;
  Member<ScriptPromiseResolver> resolver_;
};

NavigatorShare::ShareClientImpl::ShareClientImpl(
    NavigatorShare* navigator_share,
    ScriptPromiseResolver* resolver)
    : navigator_(navigator_share), resolver_(resolver) {}

void NavigatorShare::ShareClientImpl::Callback(mojom::blink::ShareError error) {
  if (navigator_)
    navigator_->clients_.erase(this);

  if (error == mojom::blink::ShareError::OK) {
    resolver_->Resolve();
  } else {
    resolver_->Reject(DOMException::Create(kAbortError, ErrorToString(error)));
  }
}

void NavigatorShare::ShareClientImpl::OnConnectionError() {
  resolver_->Reject(DOMException::Create(
      kAbortError,
      "Internal error: could not connect to Web Share interface."));
}

NavigatorShare::~NavigatorShare() = default;

NavigatorShare& NavigatorShare::From(Navigator& navigator) {
  NavigatorShare* supplement = static_cast<NavigatorShare*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorShare();
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

DEFINE_TRACE(NavigatorShare) {
  visitor->Trace(clients_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorShare::NavigatorShare() {}

const char* NavigatorShare::SupplementName() {
  return "NavigatorShare";
}

ScriptPromise NavigatorShare::share(ScriptState* script_state,
                                    const ShareData& share_data) {
  Document* doc = ToDocument(ExecutionContext::From(script_state));
  DCHECK(doc);

  if (!share_data.hasTitle() && !share_data.hasText() && !share_data.hasURL()) {
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "No known share data fields supplied. If using only new fields (other "
        "than title, text and url), you must feature-detect them first.");
    return ScriptPromise::Reject(script_state, error);
  }

  KURL full_url = doc->CompleteURL(share_data.url());
  if (!full_url.IsNull() && !full_url.IsValid()) {
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Invalid URL");
    return ScriptPromise::Reject(script_state, error);
  }

  if (!UserGestureIndicator::ProcessingUserGesture()) {
    DOMException* error = DOMException::Create(
        kNotAllowedError,
        "Must be handling a user gesture to perform a share request.");
    return ScriptPromise::RejectWithDOMException(script_state, error);
  }

  if (!service_) {
    LocalFrame* frame = doc->GetFrame();
    DCHECK(frame);
    frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service_));
    service_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
        &NavigatorShare::OnConnectionError, WrapWeakPersistent(this))));
    DCHECK(service_);
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ShareClientImpl* client = new ShareClientImpl(this, resolver);
  clients_.insert(client);
  ScriptPromise promise = resolver->Promise();

  service_->Share(share_data.hasTitle() ? share_data.title() : g_empty_string,
                  share_data.hasText() ? share_data.text() : g_empty_string,
                  full_url,
                  ConvertToBaseCallback(WTF::Bind(&ShareClientImpl::Callback,
                                                  WrapPersistent(client))));

  return promise;
}

ScriptPromise NavigatorShare::share(ScriptState* script_state,
                                    Navigator& navigator,
                                    const ShareData& share_data) {
  return From(navigator).share(script_state, share_data);
}

void NavigatorShare::OnConnectionError() {
  for (auto& client : clients_) {
    client->OnConnectionError();
  }
  clients_.clear();
  service_.reset();
}

}  // namespace blink
