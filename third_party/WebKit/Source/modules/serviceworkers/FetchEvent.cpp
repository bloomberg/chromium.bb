// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/FetchEvent.h"

#include "bindings/core/v8/ToV8ForCore.h"
#include "core/dom/ExecutionContext.h"
#include "core/timing/WorkerGlobalScopePerformance.h"
#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "modules/serviceworkers/FetchRespondWithObserver.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/network/NetworkUtils.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebFeature.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerError.h"

namespace blink {

FetchEvent* FetchEvent::Create(ScriptState* script_state,
                               const AtomicString& type,
                               const FetchEventInit& initializer) {
  return new FetchEvent(script_state, type, initializer, nullptr, nullptr,
                        false);
}

FetchEvent* FetchEvent::Create(ScriptState* script_state,
                               const AtomicString& type,
                               const FetchEventInit& initializer,
                               FetchRespondWithObserver* respond_with_observer,
                               WaitUntilObserver* wait_until_observer,
                               bool navigation_preload_sent) {
  return new FetchEvent(script_state, type, initializer, respond_with_observer,
                        wait_until_observer, navigation_preload_sent);
}

Request* FetchEvent::request() const {
  return request_;
}

String FetchEvent::clientId() const {
  return client_id_;
}

bool FetchEvent::isReload() const {
  UseCounter::Count(GetExecutionContext(), WebFeature::kFetchEventIsReload);
  return is_reload_;
}

void FetchEvent::respondWith(ScriptState* script_state,
                             ScriptPromise script_promise,
                             ExceptionState& exception_state) {
  stopImmediatePropagation();
  if (observer_)
    observer_->RespondWith(script_state, script_promise, exception_state);
}

ScriptPromise FetchEvent::preloadResponse(ScriptState* script_state) {
  return preload_response_property_->Promise(script_state->World());
}

const AtomicString& FetchEvent::InterfaceName() const {
  return EventNames::FetchEvent;
}

bool FetchEvent::HasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object while waiting for the
  // preload response. This is in order to keep the resolver of preloadResponse
  // Promise alive.
  return preload_response_property_->GetState() ==
             PreloadResponseProperty::kPending &&
         GetExecutionContext();
}

FetchEvent::FetchEvent(ScriptState* script_state,
                       const AtomicString& type,
                       const FetchEventInit& initializer,
                       FetchRespondWithObserver* respond_with_observer,
                       WaitUntilObserver* wait_until_observer,
                       bool navigation_preload_sent)
    : ExtendableEvent(type, initializer, wait_until_observer),
      ContextClient(ExecutionContext::From(script_state)),
      observer_(respond_with_observer),
      preload_response_property_(new PreloadResponseProperty(
          ExecutionContext::From(script_state),
          this,
          PreloadResponseProperty::kPreloadResponse)) {
  if (!navigation_preload_sent)
    preload_response_property_->ResolveWithUndefined();

  client_id_ = initializer.clientId();
  is_reload_ = initializer.isReload();
  if (initializer.hasRequest()) {
    ScriptState::Scope scope(script_state);
    request_ = initializer.request();
    v8::Local<v8::Value> request = ToV8(request_, script_state);
    v8::Local<v8::Value> event = ToV8(this, script_state);
    if (event.IsEmpty()) {
      // |toV8| can return an empty handle when the worker is terminating.
      // We don't want the renderer to crash in such cases.
      // TODO(yhirano): Replace this branch with an assertion when the
      // graceful shutdown mechanism is introduced.
      return;
    }
    DCHECK(event->IsObject());
    // Sets a hidden value in order to teach V8 the dependency from
    // the event to the request.
    V8PrivateProperty::GetFetchEventRequest(script_state->GetIsolate())
        .Set(event.As<v8::Object>(), request);
    // From the same reason as above, setHiddenValue can return false.
    // TODO(yhirano): Add an assertion that it returns true once the
    // graceful shutdown mechanism is introduced.
  }
}

FetchEvent::~FetchEvent() {}

void FetchEvent::OnNavigationPreloadResponse(
    ScriptState* script_state,
    std::unique_ptr<WebURLResponse> response,
    std::unique_ptr<WebDataConsumerHandle> data_consume_handle) {
  if (!script_state->ContextIsValid())
    return;
  DCHECK(preload_response_property_);
  DCHECK(!preload_response_);
  ScriptState::Scope scope(script_state);
  preload_response_ = std::move(response);
  FetchResponseData* response_data =
      data_consume_handle
          ? FetchResponseData::CreateWithBuffer(new BodyStreamBuffer(
                script_state, new BytesConsumerForDataConsumerHandle(
                                  ExecutionContext::From(script_state),
                                  std::move(data_consume_handle))))
          : FetchResponseData::Create();
  Vector<KURL> url_list(1);
  url_list[0] = preload_response_->Url();
  response_data->SetURLList(url_list);
  response_data->SetStatus(preload_response_->HttpStatusCode());
  response_data->SetStatusMessage(preload_response_->HttpStatusText());
  response_data->SetResponseTime(
      preload_response_->ToResourceResponse().ResponseTime());
  const HTTPHeaderMap& headers(
      preload_response_->ToResourceResponse().HttpHeaderFields());
  for (const auto& header : headers) {
    response_data->HeaderList()->Append(header.key, header.value);
  }
  FetchResponseData* tainted_response =
      NetworkUtils::IsRedirectResponseCode(preload_response_->HttpStatusCode())
          ? response_data->CreateOpaqueRedirectFilteredResponse()
          : response_data->CreateBasicFilteredResponse();
  preload_response_property_->Resolve(
      Response::Create(ExecutionContext::From(script_state), tainted_response));
}

void FetchEvent::OnNavigationPreloadError(
    ScriptState* script_state,
    std::unique_ptr<WebServiceWorkerError> error) {
  if (!script_state->ContextIsValid())
    return;
  DCHECK(preload_response_property_);
  if (preload_response_property_->GetState() !=
      PreloadResponseProperty::kPending) {
    return;
  }
  preload_response_property_->Reject(
      ServiceWorkerError::Take(nullptr, *error.get()));
}

void FetchEvent::OnNavigationPreloadComplete(
    WorkerGlobalScope* worker_global_scope,
    double completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  DCHECK(preload_response_);
  std::unique_ptr<WebURLResponse> response = std::move(preload_response_);
  ResourceResponse resource_response = response->ToResourceResponse();
  resource_response.SetEncodedDataLength(encoded_data_length);
  resource_response.SetEncodedBodyLength(encoded_body_length);
  resource_response.SetDecodedBodyLength(decoded_body_length);
  // According to the current spec of Resource Timing, the initiator type of
  // navigation preload request must be "other". But it may change when the spec
  // discussion is settled. https://github.com/w3c/resource-timing/issues/110
  RefPtr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
      "other", resource_response.GetResourceLoadTiming()->RequestTime(),
      false /* is_main_resource */);
  info->SetNegativeAllowed(true);
  info->SetLoadFinishTime(completion_time);
  info->SetInitialURL(request_->url());
  info->SetFinalResponse(resource_response);
  info->AddFinalTransferSize(encoded_data_length);
  WorkerGlobalScopePerformance::performance(*worker_global_scope)
      ->AddResourceTiming(*info);
}

DEFINE_TRACE(FetchEvent) {
  visitor->Trace(observer_);
  visitor->Trace(request_);
  visitor->Trace(preload_response_property_);
  ExtendableEvent::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
