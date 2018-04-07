// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/response.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker_response.h"
#include "third_party/blink/public/platform/web_cors.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/exception_state.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_form_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_search_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/response_init.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"

namespace blink {

namespace {

FetchResponseData* CreateFetchResponseDataFromWebResponse(
    ScriptState* script_state,
    const WebServiceWorkerResponse& web_response) {
  FetchResponseData* response = nullptr;
  if (web_response.Status() > 0)
    response = FetchResponseData::Create();
  else
    response = FetchResponseData::CreateNetworkErrorResponse();

  const WebVector<WebURL>& web_url_list = web_response.UrlList();
  Vector<KURL> url_list(web_url_list.size());
  std::transform(web_url_list.begin(), web_url_list.end(), url_list.begin(),
                 [](const WebURL& url) { return url; });
  response->SetURLList(url_list);
  response->SetStatus(web_response.Status());
  response->SetStatusMessage(web_response.StatusText());
  response->SetResponseTime(web_response.ResponseTime());
  response->SetCacheStorageCacheName(web_response.CacheStorageCacheName());

  for (HTTPHeaderMap::const_iterator i = web_response.Headers().begin(),
                                     end = web_response.Headers().end();
       i != end; ++i) {
    response->HeaderList()->Append(i->key, i->value);
  }

  response->ReplaceBodyStreamBuffer(new BodyStreamBuffer(
      script_state,
      new BlobBytesConsumer(ExecutionContext::From(script_state),
                            web_response.GetBlobDataHandle()),
      nullptr /* AbortSignal */));

  // Filter the response according to |webResponse|'s ResponseType.
  switch (web_response.ResponseType()) {
    case network::mojom::FetchResponseType::kBasic:
      response = response->CreateBasicFilteredResponse();
      break;
    case network::mojom::FetchResponseType::kCORS: {
      WebHTTPHeaderSet header_names;
      for (const auto& header : web_response.CorsExposedHeaderNames())
        header_names.insert(header.Ascii().data());
      response = response->CreateCORSFilteredResponse(header_names);
      break;
    }
    case network::mojom::FetchResponseType::kOpaque:
      response = response->CreateOpaqueFilteredResponse();
      break;
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      response = response->CreateOpaqueRedirectFilteredResponse();
      break;
    case network::mojom::FetchResponseType::kDefault:
      break;
    case network::mojom::FetchResponseType::kError:
      DCHECK_EQ(response->GetType(), network::mojom::FetchResponseType::kError);
      break;
  }

  return response;
}

// Checks whether |status| is a null body status.
// Spec: https://fetch.spec.whatwg.org/#null-body-status
bool IsNullBodyStatus(unsigned short status) {
  if (status == 101 || status == 204 || status == 205 || status == 304)
    return true;

  return false;
}

// Check whether |statusText| is a ByteString and
// matches the Reason-Phrase token production.
// RFC 2616: https://tools.ietf.org/html/rfc2616
// RFC 7230: https://tools.ietf.org/html/rfc7230
// "reason-phrase = *( HTAB / SP / VCHAR / obs-text )"
bool IsValidReasonPhrase(const String& status_text) {
  for (unsigned i = 0; i < status_text.length(); ++i) {
    UChar c = status_text[i];
    if (!(c == 0x09                      // HTAB
          || (0x20 <= c && c <= 0x7E)    // SP / VCHAR
          || (0x80 <= c && c <= 0xFF)))  // obs-text
      return false;
  }
  return true;
}

}  // namespace

Response* Response::Create(ScriptState* script_state,
                           ExceptionState& exception_state) {
  return Create(script_state, nullptr, String(), ResponseInit(),
                exception_state);
}

Response* Response::Create(ScriptState* script_state,
                           ScriptValue body_value,
                           const ResponseInit& init,
                           ExceptionState& exception_state) {
  v8::Local<v8::Value> body = body_value.V8Value();
  v8::Isolate* isolate = script_state->GetIsolate();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  BodyStreamBuffer* body_buffer = nullptr;
  String content_type;
  if (body_value.IsUndefined() || body_value.IsNull()) {
    // Note: The IDL processor cannot handle this situation. See
    // https://crbug.com/335871.
  } else if (V8Blob::hasInstance(body, isolate)) {
    Blob* blob = V8Blob::ToImpl(body.As<v8::Object>());
    body_buffer = new BodyStreamBuffer(
        script_state,
        new BlobBytesConsumer(execution_context, blob->GetBlobDataHandle()),
        nullptr /* AbortSignal */);
    content_type = blob->type();
  } else if (body->IsArrayBuffer()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBuffer* array_buffer = V8ArrayBuffer::ToImpl(body.As<v8::Object>());
    body_buffer = new BodyStreamBuffer(script_state,
                                       new FormDataBytesConsumer(array_buffer),
                                       nullptr /* AbortSignal */);
  } else if (body->IsArrayBufferView()) {
    // Avoid calling into V8 from the following constructor parameters, which
    // is potentially unsafe.
    DOMArrayBufferView* array_buffer_view =
        V8ArrayBufferView::ToImpl(body.As<v8::Object>());
    body_buffer = new BodyStreamBuffer(
        script_state, new FormDataBytesConsumer(array_buffer_view),
        nullptr /* AbortSignal */);
  } else if (V8FormData::hasInstance(body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8FormData::ToImpl(body.As<v8::Object>())->EncodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    content_type = AtomicString("multipart/form-data; boundary=") +
                   form_data->Boundary().data();
    body_buffer = new BodyStreamBuffer(
        script_state,
        new FormDataBytesConsumer(execution_context, std::move(form_data)),
        nullptr /* AbortSignal */);
  } else if (V8URLSearchParams::hasInstance(body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8URLSearchParams::ToImpl(body.As<v8::Object>())->ToEncodedFormData();
    body_buffer = new BodyStreamBuffer(
        script_state,
        new FormDataBytesConsumer(execution_context, std::move(form_data)),
        nullptr /* AbortSignal */);
    content_type = "application/x-www-form-urlencoded;charset=UTF-8";
  } else if (ReadableStreamOperations::IsReadableStream(script_state,
                                                        body_value)) {
    UseCounter::Count(execution_context,
                      WebFeature::kFetchResponseConstructionWithStream);
    body_buffer = new BodyStreamBuffer(script_state, body_value);
  } else {
    String string = NativeValueTraits<IDLUSVString>::NativeValue(
        isolate, body, exception_state);
    if (exception_state.HadException())
      return nullptr;
    body_buffer =
        new BodyStreamBuffer(script_state, new FormDataBytesConsumer(string),
                             nullptr /* AbortSignal */);
    content_type = "text/plain;charset=UTF-8";
  }
  return Create(script_state, body_buffer, content_type, init, exception_state);
}

Response* Response::Create(ScriptState* script_state,
                           BodyStreamBuffer* body,
                           const String& content_type,
                           const ResponseInit& init,
                           ExceptionState& exception_state) {
  unsigned short status = init.status();

  // "1. If |init|'s status member is not in the range 200 to 599, inclusive,
  //     throw a RangeError."
  if (status < 200 || 599 < status) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "status", status, 200, ExceptionMessages::kInclusiveBound, 599,
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  // "2. If |init|'s statusText member does not match the Reason-Phrase
  // token production, throw a TypeError."
  if (!IsValidReasonPhrase(init.statusText())) {
    exception_state.ThrowTypeError("Invalid statusText");
    return nullptr;
  }

  // "3. Let |r| be a new Response object, associated with a new response,
  // Headers object, and Body object."
  Response* r = new Response(ExecutionContext::From(script_state));

  // "4. Set |r|'s response's status to |init|'s status member."
  r->response_->SetStatus(init.status());

  // "5. Set |r|'s response's status message to |init|'s statusText member."
  r->response_->SetStatusMessage(AtomicString(init.statusText()));

  // "6. If |init|'s headers member is present, run these substeps:"
  if (init.hasHeaders()) {
    // "1. Empty |r|'s response's header list."
    r->response_->HeaderList()->ClearList();
    // "2. Fill |r|'s Headers object with |init|'s headers member. Rethrow
    // any exceptions."
    r->headers_->FillWith(init.headers(), exception_state);
    if (exception_state.HadException())
      return nullptr;
  }
  // "7. If body is given, run these substeps:"
  if (body) {
    // "1. If |init|'s status member is a null body status, throw a
    //     TypeError."
    // "2. Let |stream| and |Content-Type| be the result of extracting
    //     body."
    // "3. Set |r|'s response's body to |stream|."
    // "4. If |Content-Type| is non-null and |r|'s response's header list
    // contains no header named `Content-Type`, append `Content-Type`/
    // |Content-Type| to |r|'s response's header list."
    // https://fetch.spec.whatwg.org/#concept-bodyinit-extract
    // Step 3, Blob:
    // "If object's type attribute is not the empty byte sequence, set
    // Content-Type to its value."
    if (IsNullBodyStatus(status)) {
      exception_state.ThrowTypeError(
          "Response with null body status cannot have body");
      return nullptr;
    }
    r->response_->ReplaceBodyStreamBuffer(body);
    r->RefreshBody(script_state);
    if (!content_type.IsEmpty() &&
        !r->response_->HeaderList()->Has("Content-Type"))
      r->response_->HeaderList()->Append("Content-Type", content_type);
  }

  // "8. Set |r|'s MIME type to the result of extracting a MIME type
  // from |r|'s response's header list."
  r->response_->SetMIMEType(r->response_->HeaderList()->ExtractMIMEType());

  // "9. Return |r|."
  return r;
}

Response* Response::Create(ExecutionContext* context,
                           FetchResponseData* response) {
  return new Response(context, response);
}

Response* Response::Create(ScriptState* script_state,
                           const WebServiceWorkerResponse& web_response) {
  FetchResponseData* response_data =
      CreateFetchResponseDataFromWebResponse(script_state, web_response);
  return new Response(ExecutionContext::From(script_state), response_data);
}

Response* Response::error(ScriptState* script_state) {
  FetchResponseData* response_data =
      FetchResponseData::CreateNetworkErrorResponse();
  Response* r =
      new Response(ExecutionContext::From(script_state), response_data);
  r->headers_->SetGuard(Headers::kImmutableGuard);
  return r;
}

Response* Response::redirect(ScriptState* script_state,
                             const String& url,
                             unsigned short status,
                             ExceptionState& exception_state) {
  KURL parsed_url = ExecutionContext::From(script_state)->CompleteURL(url);
  if (!parsed_url.IsValid()) {
    exception_state.ThrowTypeError("Failed to parse URL from " + url);
    return nullptr;
  }

  if (!NetworkUtils::IsRedirectResponseCode(status)) {
    exception_state.ThrowRangeError("Invalid status code");
    return nullptr;
  }

  Response* r = new Response(ExecutionContext::From(script_state));
  r->headers_->SetGuard(Headers::kImmutableGuard);
  r->response_->SetStatus(status);
  r->response_->HeaderList()->Set("Location", parsed_url);

  return r;
}

String Response::type() const {
  // "The type attribute's getter must return response's type."
  switch (response_->GetType()) {
    case network::mojom::FetchResponseType::kBasic:
      return "basic";
    case network::mojom::FetchResponseType::kCORS:
      return "cors";
    case network::mojom::FetchResponseType::kDefault:
      return "default";
    case network::mojom::FetchResponseType::kError:
      return "error";
    case network::mojom::FetchResponseType::kOpaque:
      return "opaque";
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return "opaqueredirect";
  }
  NOTREACHED();
  return "";
}

String Response::url() const {
  // "The url attribute's getter must return the empty string if response's
  // url is null and response's url, serialized with the exclude fragment
  // flag set, otherwise."
  const KURL* response_url = response_->Url();
  if (!response_url)
    return g_empty_string;
  if (!response_url->HasFragmentIdentifier())
    return *response_url;
  KURL url(*response_url);
  url.RemoveFragmentIdentifier();
  return url;
}

bool Response::redirected() const {
  return response_->UrlList().size() > 1;
}

unsigned short Response::status() const {
  // "The status attribute's getter must return response's status."
  return response_->Status();
}

bool Response::ok() const {
  // "The ok attribute's getter must return true
  // if response's status is in the range 200 to 299, and false otherwise."
  return FetchUtils::IsOkStatus(status());
}

String Response::statusText() const {
  // "The statusText attribute's getter must return response's status message."
  return response_->StatusMessage();
}

Headers* Response::headers() const {
  // "The headers attribute's getter must return the associated Headers object."
  return headers_;
}

Response* Response::clone(ScriptState* script_state,
                          ExceptionState& exception_state) {
  if (IsBodyLocked() || bodyUsed()) {
    exception_state.ThrowTypeError("Response body is already used");
    return nullptr;
  }

  FetchResponseData* response = response_->Clone(script_state);
  RefreshBody(script_state);
  Headers* headers = Headers::Create(response->HeaderList());
  headers->SetGuard(headers_->GetGuard());
  return new Response(GetExecutionContext(), response, headers);
}

bool Response::HasPendingActivity() const {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return false;
  if (!InternalBodyBuffer())
    return false;
  if (InternalBodyBuffer()->HasPendingActivity())
    return true;
  return Body::HasPendingActivity();
}

void Response::PopulateWebServiceWorkerResponse(
    WebServiceWorkerResponse& response) {
  response_->PopulateWebServiceWorkerResponse(response);
}

Response::Response(ExecutionContext* context)
    : Response(context, FetchResponseData::Create()) {}

Response::Response(ExecutionContext* context, FetchResponseData* response)
    : Response(context, response, Headers::Create(response->HeaderList())) {
  headers_->SetGuard(Headers::kResponseGuard);
}

Response::Response(ExecutionContext* context,
                   FetchResponseData* response,
                   Headers* headers)
    : Body(context), response_(response), headers_(headers) {
  InstallBody();
}

bool Response::HasBody() const {
  return response_->InternalBuffer();
}

bool Response::bodyUsed() {
  return InternalBodyBuffer() && InternalBodyBuffer()->IsStreamDisturbed();
}

String Response::MimeType() const {
  return response_->MimeType();
}

String Response::ContentType() const {
  String result;
  response_->HeaderList()->Get(HTTPNames::Content_Type, result);
  return result;
}

String Response::InternalMIMEType() const {
  return response_->InternalMIMEType();
}

const Vector<KURL>& Response::InternalURLList() const {
  return response_->InternalURLList();
}

void Response::InstallBody() {
  if (!InternalBodyBuffer())
    return;
  RefreshBody(InternalBodyBuffer()->GetScriptState());
}

void Response::RefreshBody(ScriptState* script_state) {
  v8::Local<v8::Value> body_buffer = ToV8(InternalBodyBuffer(), script_state);
  v8::Local<v8::Value> response = ToV8(this, script_state);
  if (response.IsEmpty()) {
    // |toV8| can return an empty handle when the worker is terminating.
    // We don't want the renderer to crash in such cases.
    // TODO(yhirano): Delete this block after the graceful shutdown
    // mechanism is introduced.
    return;
  }
  DCHECK(response->IsObject());
  V8PrivateProperty::GetInternalBodyBuffer(script_state->GetIsolate())
      .Set(response.As<v8::Object>(), body_buffer);
}

void Response::Trace(blink::Visitor* visitor) {
  Body::Trace(visitor);
  visitor->Trace(response_);
  visitor->Trace(headers_);
}

}  // namespace blink
