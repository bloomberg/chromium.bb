// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webshare/navigator_share.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/webshare/share_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

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

bool HasFiles(const ShareData& share_data) {
  if (!RuntimeEnabledFeatures::WebShareV2Enabled() || !share_data.hasFiles())
    return false;

  const HeapVector<Member<File>>& files = share_data.files();
  return !files.IsEmpty();
}

// Returns a message for a TypeError if share(share_data) would reject with
// TypeError. https://wicg.github.io/web-share/level-2/#canshare-method
// Otherwise returns an empty string.
// Populates full_url with the result of running the URL parser on
// share_data.url
String CheckForTypeError(const Document& doc,
                         const ShareData& share_data,
                         KURL* full_url) {
  if (!share_data.hasTitle() && !share_data.hasText() && !share_data.hasURL() &&
      !HasFiles(share_data)) {
    return "No known share data fields supplied. If using only new fields "
           "(other than title, text and url), you must feature-detect "
           "them first.";
  }

  *full_url = doc.CompleteURL(share_data.url());
  if (!full_url->IsNull() && !full_url->IsValid()) {
    return "Invalid URL";
  }

  return g_empty_string;
}

}  // namespace

class NavigatorShare::ShareClientImpl final
    : public GarbageCollected<ShareClientImpl> {
 public:
  ShareClientImpl(NavigatorShare*, ScriptPromiseResolver*);

  void Callback(mojom::blink::ShareError);

  void OnConnectionError();

  void Trace(blink::Visitor* visitor) {
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
    resolver_->Reject(DOMException::Create(DOMExceptionCode::kAbortError,
                                           ErrorToString(error)));
  }
}

void NavigatorShare::ShareClientImpl::OnConnectionError() {
  resolver_->Reject(DOMException::Create(
      DOMExceptionCode::kAbortError,
      "Internal error: could not connect to Web Share interface."));
}

NavigatorShare::~NavigatorShare() = default;

NavigatorShare& NavigatorShare::From(Navigator& navigator) {
  NavigatorShare* supplement =
      Supplement<Navigator>::From<NavigatorShare>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorShare>();
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

void NavigatorShare::Trace(blink::Visitor* visitor) {
  visitor->Trace(clients_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorShare::NavigatorShare() = default;

const char NavigatorShare::kSupplementName[] = "NavigatorShare";

bool NavigatorShare::canShare(ScriptState* script_state,
                              const ShareData* share_data) {
  Document* doc = To<Document>(ExecutionContext::From(script_state));
  KURL full_url;
  return CheckForTypeError(*doc, *share_data, &full_url).IsEmpty();
}

bool NavigatorShare::canShare(ScriptState* script_state,
                              Navigator& navigator,
                              const ShareData* share_data) {
  return From(navigator).canShare(script_state, share_data);
}

ScriptPromise NavigatorShare::share(ScriptState* script_state,
                                    const ShareData* share_data) {
  Document* doc = To<Document>(ExecutionContext::From(script_state));
  KURL full_url;
  String error_message = CheckForTypeError(*doc, *share_data, &full_url);
  if (!error_message.IsEmpty()) {
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), error_message);
    return ScriptPromise::Reject(script_state, error);
  }

  if (!LocalFrame::HasTransientUserActivation(doc->GetFrame())) {
    DOMException* error = DOMException::Create(
        DOMExceptionCode::kNotAllowedError,
        "Must be handling a user gesture to perform a share request.");
    return ScriptPromise::RejectWithDOMException(script_state, error);
  }

  if (!service_) {
    LocalFrame* frame = doc->GetFrame();
    if (!frame) {
      DOMException* error =
          DOMException::Create(DOMExceptionCode::kAbortError,
                               "Internal error: document frame is missing (the "
                               "navigator may be detached).");
      return ScriptPromise::RejectWithDOMException(script_state, error);
    }

    // See https://bit.ly/2S0zRAS for task types.
    frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(
        &service_, frame->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    service_.set_connection_error_handler(WTF::Bind(
        &NavigatorShare::OnConnectionError, WrapWeakPersistent(this)));
    DCHECK(service_);
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ShareClientImpl* client =
      MakeGarbageCollected<ShareClientImpl>(this, resolver);
  clients_.insert(client);
  ScriptPromise promise = resolver->Promise();

  service_->Share(
      share_data->hasTitle() ? share_data->title() : g_empty_string,
      share_data->hasText() ? share_data->text() : g_empty_string, full_url,
      WTF::Bind(&ShareClientImpl::Callback, WrapPersistent(client)));

  return promise;
}

ScriptPromise NavigatorShare::share(ScriptState* script_state,
                                    Navigator& navigator,
                                    const ShareData* share_data) {
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
