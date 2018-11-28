// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"

#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace blink {

FetchRequestData* FetchRequestData::Create() {
  return MakeGarbageCollected<FetchRequestData>();
}

FetchRequestData* FetchRequestData::Create(
    ScriptState* script_state,
    const WebServiceWorkerRequest& web_request) {
  FetchRequestData* request = FetchRequestData::Create();
  request->url_ = web_request.Url();
  request->method_ = web_request.Method();
  for (HTTPHeaderMap::const_iterator it = web_request.Headers().begin();
       it != web_request.Headers().end(); ++it)
    request->header_list_->Append(it->key, it->value);
  if (scoped_refptr<EncodedFormData> body = web_request.Body()) {
    request->SetBuffer(new BodyStreamBuffer(
        script_state,
        MakeGarbageCollected<FormDataBytesConsumer>(
            ExecutionContext::From(script_state), std::move(body)),
        nullptr /* AbortSignal */));
  } else if (web_request.GetBlobDataHandle()) {
    request->SetBuffer(new BodyStreamBuffer(
        script_state,
        new BlobBytesConsumer(ExecutionContext::From(script_state),
                              web_request.GetBlobDataHandle()),
        nullptr /* AbortSignal */));
  }
  request->SetContext(web_request.GetRequestContext());
  request->SetReferrerString(web_request.ReferrerUrl().GetString());
  request->SetReferrerPolicy(web_request.GetReferrerPolicy());
  request->SetMode(web_request.Mode());
  request->SetCredentials(web_request.CredentialsMode());
  request->SetCacheMode(web_request.CacheMode());
  request->SetRedirect(web_request.RedirectMode());
  request->SetMIMEType(request->header_list_->ExtractMIMEType());
  request->SetIntegrity(web_request.Integrity());
  request->SetPriority(
      static_cast<ResourceLoadPriority>(web_request.Priority()));
  request->SetKeepalive(web_request.Keepalive());
  request->SetIsHistoryNavigation(web_request.IsHistoryNavigation());
  request->SetWindowId(web_request.GetWindowId());
  return request;
}

FetchRequestData* FetchRequestData::Create(
    ScriptState* script_state,
    const mojom::blink::FetchAPIRequest& fetch_api_request) {
  FetchRequestData* request = FetchRequestData::Create();
  request->url_ = fetch_api_request.url;
  request->method_ = AtomicString(fetch_api_request.method);
  for (const auto& pair : fetch_api_request.headers) {
    // TODO(leonhsl): Check sources of |fetch_api_request.headers| to make clear
    // whether we really need this filter.
    if (DeprecatedEqualIgnoringCase(pair.key, "referer"))
      continue;
    request->header_list_->Append(pair.key, pair.value);
  }
  if (fetch_api_request.blob) {
    request->SetBuffer(new BodyStreamBuffer(
        script_state,
        new BlobBytesConsumer(ExecutionContext::From(script_state),
                              fetch_api_request.blob),
        nullptr /* AbortSignal */));
  }
  request->SetContext(fetch_api_request.request_context_type);
  request->SetReferrerString(AtomicString(Referrer::NoReferrer()));
  if (fetch_api_request.referrer) {
    if (!fetch_api_request.referrer->url.IsEmpty())
      request->SetReferrerString(AtomicString(fetch_api_request.referrer->url));
    request->SetReferrerPolicy(fetch_api_request.referrer->policy);
  }
  request->SetMode(fetch_api_request.mode);
  request->SetCredentials(fetch_api_request.credentials_mode);
  request->SetCacheMode(fetch_api_request.cache_mode);
  request->SetRedirect(fetch_api_request.redirect_mode);
  request->SetMIMEType(request->header_list_->ExtractMIMEType());
  request->SetIntegrity(fetch_api_request.integrity);
  request->SetKeepalive(fetch_api_request.keepalive);
  request->SetIsHistoryNavigation(fetch_api_request.is_history_navigation);
  return request;
}

FetchRequestData* FetchRequestData::CloneExceptBody() {
  FetchRequestData* request = FetchRequestData::Create();
  request->url_ = url_;
  request->method_ = method_;
  request->header_list_ = header_list_->Clone();
  request->origin_ = origin_;
  request->same_origin_data_url_flag_ = same_origin_data_url_flag_;
  request->context_ = context_;
  request->referrer_string_ = referrer_string_;
  request->referrer_policy_ = referrer_policy_;
  request->mode_ = mode_;
  request->credentials_ = credentials_;
  request->cache_mode_ = cache_mode_;
  request->redirect_ = redirect_;
  request->response_tainting_ = response_tainting_;
  request->mime_type_ = mime_type_;
  request->integrity_ = integrity_;
  request->priority_ = priority_;
  request->importance_ = importance_;
  request->keepalive_ = keepalive_;
  request->is_history_navigation_ = is_history_navigation_;
  request->window_id_ = window_id_;
  return request;
}

FetchRequestData* FetchRequestData::Clone(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  FetchRequestData* request = FetchRequestData::CloneExceptBody();
  if (buffer_) {
    BodyStreamBuffer* new1 = nullptr;
    BodyStreamBuffer* new2 = nullptr;
    buffer_->Tee(&new1, &new2, exception_state);
    if (exception_state.HadException())
      return nullptr;
    buffer_ = new1;
    request->buffer_ = new2;
  }
  if (url_loader_factory_) {
    url_loader_factory_->Clone(MakeRequest(&request->url_loader_factory_));
  }
  return request;
}

FetchRequestData* FetchRequestData::Pass(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  FetchRequestData* request = FetchRequestData::CloneExceptBody();
  if (buffer_) {
    request->buffer_ = buffer_;
    buffer_ = new BodyStreamBuffer(script_state, BytesConsumer::CreateClosed(),
                                   nullptr /* AbortSignal */);
    buffer_->CloseAndLockAndDisturb(exception_state);
    if (exception_state.HadException())
      return nullptr;
  }
  request->url_loader_factory_ = std::move(url_loader_factory_);
  return request;
}

FetchRequestData::~FetchRequestData() {}

FetchRequestData::FetchRequestData()
    : method_(http_names::kGET),
      header_list_(FetchHeaderList::Create()),
      context_(mojom::RequestContextType::UNSPECIFIED),
      same_origin_data_url_flag_(false),
      referrer_string_(Referrer::ClientReferrerString()),
      referrer_policy_(network::mojom::ReferrerPolicy::kDefault),
      mode_(network::mojom::FetchRequestMode::kNoCors),
      credentials_(network::mojom::FetchCredentialsMode::kOmit),
      cache_mode_(mojom::FetchCacheMode::kDefault),
      redirect_(network::mojom::FetchRedirectMode::kFollow),
      importance_(mojom::FetchImportanceMode::kImportanceAuto),
      response_tainting_(kBasicTainting),
      priority_(ResourceLoadPriority::kUnresolved),
      keepalive_(false) {}

void FetchRequestData::Trace(blink::Visitor* visitor) {
  visitor->Trace(buffer_);
  visitor->Trace(header_list_);
}

}  // namespace blink
