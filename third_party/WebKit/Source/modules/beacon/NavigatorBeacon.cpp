// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/beacon/NavigatorBeacon.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/array_buffer_view_or_blob_or_string_or_form_data.h"
#include "core/dom/ExceptionCode.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/forms/FormData.h"
#include "core/loader/PingLoader.h"
#include "core/typed_arrays/DOMArrayBufferView.h"
#include "platform/bindings/ScriptState.h"
#include "platform/loader/cors/CORS.h"

namespace blink {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorBeacon::~NavigatorBeacon() = default;

void NavigatorBeacon::Trace(blink::Visitor* visitor) {
  Supplement<Navigator>::Trace(visitor);
}

const char NavigatorBeacon::kSupplementName[] = "NavigatorBeacon";

NavigatorBeacon& NavigatorBeacon::From(Navigator& navigator) {
  NavigatorBeacon* supplement =
      Supplement<Navigator>::From<NavigatorBeacon>(navigator);
  if (!supplement) {
    supplement = new NavigatorBeacon(navigator);
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

  // If detached from frame, do not allow sending a Beacon.
  if (!GetSupplementable()->GetFrame())
    return false;

  return true;
}

bool NavigatorBeacon::sendBeacon(
    ScriptState* script_state,
    Navigator& navigator,
    const String& urlstring,
    const ArrayBufferViewOrBlobOrStringOrFormData& data,
    ExceptionState& exception_state) {
  return NavigatorBeacon::From(navigator).SendBeaconImpl(
      script_state, urlstring, data, exception_state);
}

bool NavigatorBeacon::SendBeaconImpl(
    ScriptState* script_state,
    const String& urlstring,
    const ArrayBufferViewOrBlobOrStringOrFormData& data,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  KURL url = context->CompleteURL(urlstring);
  if (!CanSendBeacon(context, url, exception_state))
    return false;

  bool allowed;

  if (data.IsArrayBufferView()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url,
                                     data.GetAsArrayBufferView().View());
  } else if (data.IsBlob()) {
    Blob* blob = data.GetAsBlob();
    if (!CORS::IsCORSSafelistedContentType(blob->type())) {
      UseCounter::Count(context,
                        WebFeature::kSendBeaconWithNonSimpleContentType);
      if (RuntimeEnabledFeatures::
              SendBeaconThrowForBlobWithNonSimpleTypeEnabled()) {
        exception_state.ThrowSecurityError(
            "sendBeacon() with a Blob whose type is not any of the "
            "CORS-safelisted values for the Content-Type request header is "
            "disabled temporarily. See http://crbug.com/490015 for details.");
        return false;
      }
    }
    allowed =
        PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url, blob);
  } else if (data.IsString()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url,
                                     data.GetAsString());
  } else if (data.IsFormData()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url,
                                     data.GetAsFormData());
  } else {
    allowed =
        PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url, String());
  }

  if (!allowed) {
    UseCounter::Count(context, WebFeature::kSendBeaconQuotaExceeded);
    return false;
  }

  return true;
}

}  // namespace blink
