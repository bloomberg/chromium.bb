// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ForeignFetchRespondWithObserver.h"

#include "bindings/modules/v8/V8ForeignFetchResponse.h"
#include "modules/fetch/Response.h"
#include "modules/serviceworkers/ForeignFetchResponse.h"
#include "public/platform/WebCORS.h"

namespace blink {

ForeignFetchRespondWithObserver* ForeignFetchRespondWithObserver::Create(
    ExecutionContext* context,
    int event_id,
    const KURL& request_url,
    WebURLRequest::FetchRequestMode request_mode,
    WebURLRequest::FetchRedirectMode redirect_mode,
    WebURLRequest::FrameType frame_type,
    WebURLRequest::RequestContext request_context,
    PassRefPtr<SecurityOrigin> request_origin,
    WaitUntilObserver* observer) {
  return new ForeignFetchRespondWithObserver(
      context, event_id, request_url, request_mode, redirect_mode, frame_type,
      request_context, std::move(request_origin), observer);
}

void ForeignFetchRespondWithObserver::OnResponseFulfilled(
    const ScriptValue& value) {
  DCHECK(GetExecutionContext());
  ExceptionState exception_state(value.GetIsolate(),
                                 ExceptionState::kUnknownContext,
                                 "ForeignFetchEvent", "respondWith");
  ForeignFetchResponse foreign_fetch_response =
      ScriptValue::To<ForeignFetchResponse>(ToIsolate(GetExecutionContext()),
                                            value, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    OnResponseRejected(kWebServiceWorkerResponseErrorNoForeignFetchResponse);
    return;
  }

  Response* response = foreign_fetch_response.response();
  const FetchResponseData* internal_response = response->GetResponse();
  const bool is_opaque =
      internal_response->GetType() == FetchResponseData::kOpaqueType ||
      internal_response->GetType() == FetchResponseData::kOpaqueRedirectType;
  if (internal_response->GetType() != FetchResponseData::kDefaultType)
    internal_response = internal_response->InternalResponse();

  if (!foreign_fetch_response.hasOrigin()) {
    if (foreign_fetch_response.hasHeaders() &&
        !foreign_fetch_response.headers().IsEmpty()) {
      OnResponseRejected(
          kWebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin);
      return;
    }

    // If response isn't already opaque, make it opaque.
    if (!is_opaque) {
      FetchResponseData* opaque_data =
          internal_response->CreateOpaqueFilteredResponse();
      response = Response::Create(GetExecutionContext(), opaque_data);
    }
  } else if (request_origin_->ToString() != foreign_fetch_response.origin()) {
    OnResponseRejected(
        kWebServiceWorkerResponseErrorForeignFetchMismatchedOrigin);
    return;
  } else if (!is_opaque) {
    WebHTTPHeaderSet headers;
    if (foreign_fetch_response.hasHeaders()) {
      for (const String& header : foreign_fetch_response.headers())
        headers.emplace(header.Ascii().data(), header.Ascii().length());
      if (response->GetResponse()->GetType() == FetchResponseData::kCORSType) {
        const WebHTTPHeaderSet& existing_headers =
            response->GetResponse()->CorsExposedHeaderNames();
        for (WebHTTPHeaderSet::iterator it = headers.begin();
             it != headers.end();) {
          if (existing_headers.find(*it) == existing_headers.end()) {
            it = headers.erase(it);
          } else {
            ++it;
          }
        }
      }
    }
    FetchResponseData* response_data =
        internal_response->CreateCORSFilteredResponse(headers);
    response = Response::Create(GetExecutionContext(), response_data);
  }

  FetchRespondWithObserver::OnResponseFulfilled(
      ScriptValue::From(value.GetScriptState(), response));
}

ForeignFetchRespondWithObserver::ForeignFetchRespondWithObserver(
    ExecutionContext* context,
    int event_id,
    const KURL& request_url,
    WebURLRequest::FetchRequestMode request_mode,
    WebURLRequest::FetchRedirectMode redirect_mode,
    WebURLRequest::FrameType frame_type,
    WebURLRequest::RequestContext request_context,
    PassRefPtr<SecurityOrigin> request_origin,
    WaitUntilObserver* observer)
    : FetchRespondWithObserver(context,
                               event_id,
                               request_url,
                               request_mode,
                               redirect_mode,
                               frame_type,
                               request_context,
                               observer),
      request_origin_(std::move(request_origin)) {}

}  // namespace blink
