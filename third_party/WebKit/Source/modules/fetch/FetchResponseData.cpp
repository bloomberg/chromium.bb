// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchResponseData.h"

#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchHeaderList.h"
#include "platform/bindings/ScriptState.h"
#include "platform/http_names.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

namespace blink {

namespace {

network::mojom::FetchResponseType FetchTypeToWebType(
    FetchResponseData::Type fetch_type) {
  network::mojom::FetchResponseType web_type =
      network::mojom::FetchResponseType::kDefault;
  switch (fetch_type) {
    case FetchResponseData::kBasicType:
      web_type = network::mojom::FetchResponseType::kBasic;
      break;
    case FetchResponseData::kCORSType:
      web_type = network::mojom::FetchResponseType::kCORS;
      break;
    case FetchResponseData::kDefaultType:
      web_type = network::mojom::FetchResponseType::kDefault;
      break;
    case FetchResponseData::kErrorType:
      web_type = network::mojom::FetchResponseType::kError;
      break;
    case FetchResponseData::kOpaqueType:
      web_type = network::mojom::FetchResponseType::kOpaque;
      break;
    case FetchResponseData::kOpaqueRedirectType:
      web_type = network::mojom::FetchResponseType::kOpaqueRedirect;
      break;
  }
  return web_type;
}

WebVector<WebString> HeaderSetToWebVector(const WebHTTPHeaderSet& headers) {
  // Can't just pass *headers to the WebVector constructor because HashSet
  // iterators are not stl iterator compatible.
  WebVector<WebString> result(static_cast<size_t>(headers.size()));
  int idx = 0;
  for (const auto& header : headers)
    result[idx++] = WebString::FromASCII(header);
  return result;
}

}  // namespace

FetchResponseData* FetchResponseData::Create() {
  // "Unless stated otherwise, a response's url is null, status is 200, status
  // message is `OK`, header list is an empty header list, and body is null."
  return new FetchResponseData(kDefaultType, 200, "OK");
}

FetchResponseData* FetchResponseData::CreateNetworkErrorResponse() {
  // "A network error is a response whose status is always 0, status message
  // is always the empty byte sequence, header list is aways an empty list,
  // and body is always null."
  return new FetchResponseData(kErrorType, 0, "");
}

FetchResponseData* FetchResponseData::CreateWithBuffer(
    BodyStreamBuffer* buffer) {
  FetchResponseData* response = FetchResponseData::Create();
  response->buffer_ = buffer;
  return response;
}

FetchResponseData* FetchResponseData::CreateBasicFilteredResponse() const {
  DCHECK_EQ(type_, kDefaultType);
  // "A basic filtered response is a filtered response whose type is |basic|,
  // header list excludes any headers in internal response's header list whose
  // name is `Set-Cookie` or `Set-Cookie2`."
  FetchResponseData* response =
      new FetchResponseData(kBasicType, status_, status_message_);
  response->SetURLList(url_list_);
  for (const auto& header : header_list_->List()) {
    if (FetchUtils::IsForbiddenResponseHeaderName(header.first))
      continue;
    response->header_list_->Append(header.first, header.second);
  }
  response->buffer_ = buffer_;
  response->mime_type_ = mime_type_;
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateCORSFilteredResponse() const {
  DCHECK_EQ(type_, kDefaultType);
  WebHTTPHeaderSet access_control_expose_header_set;
  String access_control_expose_headers;
  if (header_list_->Get(HTTPNames::Access_Control_Expose_Headers,
                        access_control_expose_headers)) {
    WebCORS::ParseAccessControlExposeHeadersAllowList(
        access_control_expose_headers, access_control_expose_header_set);
  }
  return CreateCORSFilteredResponse(access_control_expose_header_set);
}

FetchResponseData* FetchResponseData::CreateCORSFilteredResponse(
    const WebHTTPHeaderSet& exposed_headers) const {
  DCHECK_EQ(type_, kDefaultType);
  // "A CORS filtered response is a filtered response whose type is |CORS|,
  // header list excludes all headers in internal response's header list,
  // except those whose name is either one of `Cache-Control`,
  // `Content-Language`, `Content-Type`, `Expires`, `Last-Modified`, and
  // `Pragma`, and except those whose name is one of the values resulting from
  // parsing `Access-Control-Expose-Headers` in internal response's header
  // list."
  FetchResponseData* response =
      new FetchResponseData(kCORSType, status_, status_message_);
  response->SetURLList(url_list_);
  for (const auto& header : header_list_->List()) {
    const String& name = header.first;
    const bool explicitly_exposed =
        exposed_headers.find(name.Ascii().data()) != exposed_headers.end();
    if (WebCORS::IsOnAccessControlResponseHeaderWhitelist(name) ||
        (explicitly_exposed &&
         !FetchUtils::IsForbiddenResponseHeaderName(name))) {
      if (explicitly_exposed) {
        response->cors_exposed_header_names_.emplace(name.Ascii().data(),
                                                     name.Ascii().length());
      }
      response->header_list_->Append(name, header.second);
    }
  }
  response->buffer_ = buffer_;
  response->mime_type_ = mime_type_;
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateOpaqueFilteredResponse() const {
  DCHECK_EQ(type_, kDefaultType);
  // "An opaque filtered response is a filtered response whose type is
  // 'opaque', url list is the empty list, status is 0, status message is the
  // empty byte sequence, header list is the empty list, body is null, and
  // cache state is 'none'."
  //
  // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque
  FetchResponseData* response = new FetchResponseData(kOpaqueType, 0, "");
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateOpaqueRedirectFilteredResponse()
    const {
  DCHECK_EQ(type_, kDefaultType);
  // "An opaque filtered response is a filtered response whose type is
  // 'opaqueredirect', status is 0, status message is the empty byte sequence,
  // header list is the empty list, body is null, and cache state is 'none'."
  //
  // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque-redirect
  FetchResponseData* response =
      new FetchResponseData(kOpaqueRedirectType, 0, "");
  response->SetURLList(url_list_);
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

const KURL* FetchResponseData::Url() const {
  // "A response has an associated url. It is a pointer to the last response URL
  // in response’s url list and null if response’s url list is the empty list."
  if (url_list_.IsEmpty())
    return nullptr;
  return &url_list_.back();
}

String FetchResponseData::MimeType() const {
  return mime_type_;
}

BodyStreamBuffer* FetchResponseData::InternalBuffer() const {
  if (internal_response_) {
    return internal_response_->buffer_;
  }
  return buffer_;
}

String FetchResponseData::InternalMIMEType() const {
  if (internal_response_) {
    return internal_response_->MimeType();
  }
  return mime_type_;
}

void FetchResponseData::SetURLList(const Vector<KURL>& url_list) {
  url_list_ = url_list;
}

const Vector<KURL>& FetchResponseData::InternalURLList() const {
  if (internal_response_) {
    return internal_response_->url_list_;
  }
  return url_list_;
}

FetchResponseData* FetchResponseData::Clone(ScriptState* script_state) {
  FetchResponseData* new_response = Create();
  new_response->type_ = type_;
  if (termination_reason_) {
    new_response->termination_reason_ = WTF::WrapUnique(new TerminationReason);
    *new_response->termination_reason_ = *termination_reason_;
  }
  new_response->SetURLList(url_list_);
  new_response->status_ = status_;
  new_response->status_message_ = status_message_;
  new_response->header_list_ = header_list_->Clone();
  new_response->mime_type_ = mime_type_;
  new_response->response_time_ = response_time_;
  new_response->cache_storage_cache_name_ = cache_storage_cache_name_;
  new_response->cors_exposed_header_names_ = cors_exposed_header_names_;

  switch (type_) {
    case kBasicType:
    case kCORSType:
      DCHECK(internal_response_);
      DCHECK_EQ(buffer_, internal_response_->buffer_);
      DCHECK_EQ(internal_response_->type_, kDefaultType);
      new_response->internal_response_ =
          internal_response_->Clone(script_state);
      buffer_ = internal_response_->buffer_;
      new_response->buffer_ = new_response->internal_response_->buffer_;
      break;
    case kDefaultType: {
      DCHECK(!internal_response_);
      if (buffer_) {
        BodyStreamBuffer* new1 = nullptr;
        BodyStreamBuffer* new2 = nullptr;
        buffer_->Tee(&new1, &new2);
        buffer_ = new1;
        new_response->buffer_ = new2;
      }
      break;
    }
    case kErrorType:
      DCHECK(!internal_response_);
      DCHECK(!buffer_);
      break;
    case kOpaqueType:
    case kOpaqueRedirectType:
      DCHECK(internal_response_);
      DCHECK(!buffer_);
      DCHECK_EQ(internal_response_->type_, kDefaultType);
      new_response->internal_response_ =
          internal_response_->Clone(script_state);
      break;
  }
  return new_response;
}

void FetchResponseData::PopulateWebServiceWorkerResponse(
    WebServiceWorkerResponse& response) {
  if (internal_response_) {
    internal_response_->PopulateWebServiceWorkerResponse(response);
    response.SetResponseType(FetchTypeToWebType(type_));
    response.SetCorsExposedHeaderNames(
        HeaderSetToWebVector(cors_exposed_header_names_));
    return;
  }
  response.SetURLList(url_list_);
  response.SetStatus(Status());
  response.SetStatusText(StatusMessage());
  response.SetResponseType(FetchTypeToWebType(type_));
  response.SetResponseTime(ResponseTime());
  response.SetCacheStorageCacheName(CacheStorageCacheName());
  response.SetCorsExposedHeaderNames(
      HeaderSetToWebVector(cors_exposed_header_names_));
  for (const auto& header : HeaderList()->List()) {
    response.AppendHeader(header.first, header.second);
  }
}

FetchResponseData::FetchResponseData(Type type,
                                     unsigned short status,
                                     AtomicString status_message)
    : type_(type),
      status_(status),
      status_message_(status_message),
      header_list_(FetchHeaderList::Create()) {}

void FetchResponseData::ReplaceBodyStreamBuffer(BodyStreamBuffer* buffer) {
  if (type_ == kBasicType || type_ == kCORSType) {
    DCHECK(internal_response_);
    internal_response_->buffer_ = buffer;
    buffer_ = buffer;
  } else if (type_ == kDefaultType) {
    DCHECK(!internal_response_);
    buffer_ = buffer;
  }
}

void FetchResponseData::Trace(blink::Visitor* visitor) {
  visitor->Trace(header_list_);
  visitor->Trace(internal_response_);
  visitor->Trace(buffer_);
}

}  // namespace blink
