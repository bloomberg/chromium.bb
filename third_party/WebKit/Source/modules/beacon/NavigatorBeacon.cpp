// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/beacon/NavigatorBeacon.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/ArrayBufferViewOrBlobOrStringOrFormData.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/FormData.h"
#include "core/loader/PingLoader.h"
#include "core/typed_arrays/DOMArrayBufferView.h"
#include "platform/bindings/ScriptState.h"
#include "platform/loader/fetch/FetchUtils.h"

namespace blink {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : Supplement<Navigator>(navigator), transmitted_bytes_(0) {}

NavigatorBeacon::~NavigatorBeacon() {}

DEFINE_TRACE(NavigatorBeacon) {
  Supplement<Navigator>::Trace(visitor);
}

const char* NavigatorBeacon::SupplementName() {
  return "NavigatorBeacon";
}

NavigatorBeacon& NavigatorBeacon::From(Navigator& navigator) {
  NavigatorBeacon* supplement = static_cast<NavigatorBeacon*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorBeacon(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
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

// Determine the remaining size allowance for Beacon transmissions.
// If (-1) is returned, a no limit policy is in place, otherwise
// it is the max size (in bytes) of a beacon request.
//
// The loader takes the allowance into account once the Beacon
// payload size has been determined, deciding if the transmission
// will be allowed to go ahead or not.
int NavigatorBeacon::MaxAllowance() const {
  DCHECK(GetSupplementable()->GetFrame());
  const Settings* settings = GetSupplementable()->GetFrame()->GetSettings();
  if (settings) {
    int max_allowed = settings->GetMaxBeaconTransmission();
    // Any negative value represent no max limit.
    if (max_allowed < 0)
      return -1;
    if (static_cast<size_t>(max_allowed) <= transmitted_bytes_)
      return 0;
    return max_allowed - static_cast<int>(transmitted_bytes_);
  }
  return -1;
}

void NavigatorBeacon::AddTransmittedBytes(size_t sent_bytes) {
  transmitted_bytes_ += sent_bytes;
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

  int allowance = MaxAllowance();
  size_t beacon_size = 0;
  bool allowed;

  if (data.isArrayBufferView()) {
    allowed =
        PingLoader::SendBeacon(GetSupplementable()->GetFrame(), allowance, url,
                               data.getAsArrayBufferView().View(), beacon_size);
  } else if (data.isBlob()) {
    Blob* blob = data.getAsBlob();
    if (!FetchUtils::IsCORSSafelistedContentType(AtomicString(blob->type()))) {
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
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), allowance,
                                     url, blob, beacon_size);
  } else if (data.isString()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), allowance,
                                     url, data.getAsString(), beacon_size);
  } else if (data.isFormData()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), allowance,
                                     url, data.getAsFormData(), beacon_size);
  } else {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), allowance,
                                     url, String(), beacon_size);
  }

  if (!allowed) {
    UseCounter::Count(context, WebFeature::kSendBeaconQuotaExceeded);
    return false;
  }

  // Only accumulate transmission size if a limit is imposed.
  if (allowance >= 0)
    AddTransmittedBytes(beacon_size);
  return true;
}

}  // namespace blink
