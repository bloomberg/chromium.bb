// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/beacon/navigator_beacon.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_formdata_readablestream_urlsearchparams_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/readable_stream_or_xml_http_request_body_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"

namespace blink {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorBeacon::~NavigatorBeacon() = default;

void NavigatorBeacon::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
}

const char NavigatorBeacon::kSupplementName[] = "NavigatorBeacon";

NavigatorBeacon& NavigatorBeacon::From(Navigator& navigator) {
  NavigatorBeacon* supplement =
      Supplement<Navigator>::From<NavigatorBeacon>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorBeacon>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

bool NavigatorBeacon::CanSendBeacon(ExecutionContext* context,
                                    const KURL& url,
                                    ExceptionState& exception_state) {
  if (!url.IsValid()) {
    exception_state.ThrowTypeError(
        "The URL argument is ill-formed or unsupported.");
    return false;
  }
  // For now, only support HTTP and related.
  if (!url.ProtocolIsInHTTPFamily()) {
    exception_state.ThrowTypeError("Beacons are only supported over HTTP(S).");
    return false;
  }

  // If detached, do not allow sending a Beacon.
  return GetSupplementable()->DomWindow();
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
bool NavigatorBeacon::sendBeacon(
    ScriptState* script_state,
    Navigator& navigator,
    const String& url_string,
    const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data,
    ExceptionState& exception_state) {
  return NavigatorBeacon::From(navigator).SendBeaconImpl(
      script_state, url_string, data, exception_state);
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
bool NavigatorBeacon::sendBeacon(
    ScriptState* script_state,
    Navigator& navigator,
    const String& urlstring,
    const ReadableStreamOrBlobOrArrayBufferOrArrayBufferViewOrFormDataOrURLSearchParamsOrUSVString&
        data,
    ExceptionState& exception_state) {
  return NavigatorBeacon::From(navigator).SendBeaconImpl(
      script_state, urlstring, data, exception_state);
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
bool NavigatorBeacon::SendBeaconImpl(
    ScriptState* script_state,
    const String& url_string,
    const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  KURL url = execution_context->CompleteURL(url_string);
  if (!CanSendBeacon(execution_context, url, exception_state)) {
    // TODO(crbug.com/1161996): Remove this VLOG once the investigation is done.
    VLOG(1) << "Cannot send a beacon to " << url.ElidedString()
            << ", initiator = " << execution_context->Url();
    return false;
  }

  // TODO(crbug.com/1161996): Remove this VLOG once the investigation is done.
  VLOG(1) << "Send a beacon to " << url.ElidedString()
          << ", initiator = " << execution_context->Url();

  bool allowed;
  LocalFrame* frame = GetSupplementable()->DomWindow()->GetFrame();
  if (data) {
    switch (data->GetContentType()) {
      case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
          kArrayBuffer: {
        auto* data_buffer = data->GetAsArrayBuffer();
        if (!base::CheckedNumeric<wtf_size_t>(data_buffer->ByteLength())
                 .IsValid()) {
          // At the moment the PingLoader::SendBeacon implementation cannot deal
          // with huge ArrayBuffers.
          exception_state.ThrowRangeError(
              "The data provided to sendBeacon() exceeds the maximally "
              "possible length, which is 4294967295.");
          return false;
        }
        allowed =
            PingLoader::SendBeacon(*script_state, frame, url, data_buffer);
        break;
      }
      case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
          kArrayBufferView: {
        auto* data_view = data->GetAsArrayBufferView().Get();
        if (!base::CheckedNumeric<wtf_size_t>(data_view->byteLength())
                 .IsValid()) {
          // At the moment the PingLoader::SendBeacon implementation cannot deal
          // with huge ArrayBuffers.
          exception_state.ThrowRangeError(
              "The data provided to sendBeacon() exceeds the maximally "
              "possible length, which is 4294967295.");
          return false;
        }
        allowed = PingLoader::SendBeacon(*script_state, frame, url, data_view);
        break;
      }
      case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::kBlob:
        allowed = PingLoader::SendBeacon(*script_state, frame, url,
                                         data->GetAsBlob());
        break;
      case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
          kFormData:
        allowed = PingLoader::SendBeacon(*script_state, frame, url,
                                         data->GetAsFormData());
        break;
      case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
          kReadableStream:
        exception_state.ThrowTypeError(
            "sendBeacon cannot have a ReadableStream body.");
        return false;
      case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
          kURLSearchParams:
        allowed = PingLoader::SendBeacon(*script_state, frame, url,
                                         data->GetAsURLSearchParams());
        break;
      case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
          kUSVString:
        allowed = PingLoader::SendBeacon(*script_state, frame, url,
                                         data->GetAsUSVString());
        break;
    }
  } else {
    allowed = PingLoader::SendBeacon(*script_state, frame, url, String());
  }

  if (!allowed) {
    UseCounter::Count(execution_context, WebFeature::kSendBeaconQuotaExceeded);
  }

  return allowed;
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
bool NavigatorBeacon::SendBeaconImpl(
    ScriptState* script_state,
    const String& urlstring,
    const ReadableStreamOrBlobOrArrayBufferOrArrayBufferViewOrFormDataOrURLSearchParamsOrUSVString&
        data,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  KURL url = context->CompleteURL(urlstring);
  if (!CanSendBeacon(context, url, exception_state)) {
    // TODO(crbug.com/1161996): Remove this VLOG once the investigation is done.
    VLOG(1) << "Cannot send a beacon to " << url.ElidedString()
            << ", initiator = " << context->Url();
    return false;
  }

  // TODO(crbug.com/1161996): Remove this VLOG once the investigation is done.
  VLOG(1) << "Send a beacon to " << url.ElidedString()
          << ", initiator = " << context->Url();
  bool allowed;

  LocalFrame* frame = GetSupplementable()->DomWindow()->GetFrame();
  if (data.IsArrayBuffer()) {
    auto* data_buffer = data.GetAsArrayBuffer();
    if (!base::CheckedNumeric<wtf_size_t>(data_buffer->ByteLength())
             .IsValid()) {
      // At the moment the PingLoader::SendBeacon implementation cannot deal
      // with huge ArrayBuffers.
      exception_state.ThrowRangeError(
          "The data provided to sendBeacon() exceeds the maximally possible "
          "length, which is 4294967295.");
      return false;
    }
    allowed = PingLoader::SendBeacon(*script_state, frame, url, data_buffer);
  } else if (data.IsArrayBufferView()) {
    auto* data_view = data.GetAsArrayBufferView().Get();
    if (!base::CheckedNumeric<wtf_size_t>(data_view->byteLength()).IsValid()) {
      // At the moment the PingLoader::SendBeacon implementation cannot deal
      // with huge ArrayBuffers.
      exception_state.ThrowRangeError(
          "The data provided to sendBeacon() exceeds the maximally possible "
          "length, which is 4294967295.");
      return false;
    }
    allowed = PingLoader::SendBeacon(*script_state, frame, url, data_view);
  } else if (data.IsBlob()) {
    Blob* blob = data.GetAsBlob();
    allowed = PingLoader::SendBeacon(*script_state, frame, url, blob);
  } else if (data.IsUSVString()) {
    allowed = PingLoader::SendBeacon(*script_state, frame, url,
                                     data.GetAsUSVString());
  } else if (data.IsFormData()) {
    allowed =
        PingLoader::SendBeacon(*script_state, frame, url, data.GetAsFormData());
  } else if (data.IsURLSearchParams()) {
    allowed = PingLoader::SendBeacon(*script_state, frame, url,
                                     data.GetAsURLSearchParams());
  } else if (data.IsReadableStream()) {
    exception_state.ThrowTypeError(
        "sendBeacon cannot have a ReadableStream body.");
    return false;
  } else {
    allowed = PingLoader::SendBeacon(*script_state, frame, url, String());
  }

  if (!allowed) {
    UseCounter::Count(context, WebFeature::kSendBeaconQuotaExceeded);
    return false;
  }

  return true;
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

}  // namespace blink
