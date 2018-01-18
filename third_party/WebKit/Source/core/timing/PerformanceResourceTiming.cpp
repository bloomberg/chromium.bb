/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/timing/PerformanceResourceTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/timing/PerformanceBase.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "public/platform/WebResourceTimingInfo.h"

namespace blink {

PerformanceResourceTiming::PerformanceResourceTiming(
    const WebResourceTimingInfo& info,
    double time_origin,
    const AtomicString& initiator_type)
    : PerformanceEntry(info.name,
                       "resource",
                       PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
                           time_origin,
                           info.start_time,
                           info.allow_negative_values),
                       PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
                           time_origin,
                           info.finish_time,
                           info.allow_negative_values)),
      initiator_type_(initiator_type),
      alpn_negotiated_protocol_(
          static_cast<String>(info.alpn_negotiated_protocol)),
      connection_info_(static_cast<String>(info.connection_info)),
      time_origin_(time_origin),
      timing_(info.timing),
      last_redirect_end_time_(info.last_redirect_end_time),
      finish_time_(info.finish_time),
      transfer_size_(info.transfer_size),
      encoded_body_size_(info.encoded_body_size),
      decoded_body_size_(info.decoded_body_size),
      did_reuse_connection_(info.did_reuse_connection),
      allow_timing_details_(info.allow_timing_details),
      allow_redirect_details_(info.allow_redirect_details),
      allow_negative_value_(info.allow_negative_values),
      server_timing_(
          PerformanceServerTiming::FromParsedServerTiming(info.server_timing)) {
}

// This constructor is for PerformanceNavigationTiming.
PerformanceResourceTiming::PerformanceResourceTiming(
    const String& name,
    const String& entry_type,
    double time_origin,
    const WebVector<WebServerTimingInfo>& server_timing)
    : PerformanceEntry(name, entry_type, 0.0, 0.0),
      time_origin_(time_origin),
      server_timing_(
          PerformanceServerTiming::FromParsedServerTiming(server_timing)) {}

PerformanceResourceTiming::~PerformanceResourceTiming() = default;

ResourceLoadTiming* PerformanceResourceTiming::GetResourceLoadTiming() const {
  return timing_.get();
}

bool PerformanceResourceTiming::AllowTimingDetails() const {
  return allow_timing_details_;
}

bool PerformanceResourceTiming::DidReuseConnection() const {
  return did_reuse_connection_;
}

unsigned long long PerformanceResourceTiming::GetTransferSize() const {
  return transfer_size_;
}

unsigned long long PerformanceResourceTiming::GetEncodedBodySize() const {
  return encoded_body_size_;
}

unsigned long long PerformanceResourceTiming::GetDecodedBodySize() const {
  return decoded_body_size_;
}

AtomicString PerformanceResourceTiming::initiatorType() const {
  return initiator_type_;
}

AtomicString PerformanceResourceTiming::AlpnNegotiatedProtocol() const {
  return alpn_negotiated_protocol_;
}

AtomicString PerformanceResourceTiming::ConnectionInfo() const {
  return connection_info_;
}

AtomicString PerformanceResourceTiming::GetNextHopProtocol(
    const AtomicString& alpn_negotiated_protocol,
    const AtomicString& connection_info) {
  // Fallback to connection_info when alpn_negotiated_protocol is unknown.
  AtomicString returnedProtocol = (alpn_negotiated_protocol == "unknown")
                                      ? connection_info
                                      : alpn_negotiated_protocol;
  // If connection_info is also unknown, return empty string.
  // (https://github.com/w3c/navigation-timing/issues/71)
  returnedProtocol = (returnedProtocol == "unknown") ? "" : returnedProtocol;
  // If the protocol is http over quic (e.g. http/2+quic/37), convert it to the
  // alpn id "hq". (https://github.com/w3c/navigation-timing/issues/71)
  if (returnedProtocol.Contains("quic"))
    returnedProtocol = "hq";

  return returnedProtocol;
}

AtomicString PerformanceResourceTiming::nextHopProtocol() const {
  return PerformanceResourceTiming::GetNextHopProtocol(AlpnNegotiatedProtocol(),
                                                       ConnectionInfo());
}

DOMHighResTimeStamp PerformanceResourceTiming::workerStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->WorkerStart() == 0.0)
    return 0.0;

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->WorkerStart(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::WorkerReady() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->WorkerReady() == 0.0)
    return 0.0;

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->WorkerReady(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectStart() const {
  if (!last_redirect_end_time_ || !allow_redirect_details_)
    return 0.0;

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectEnd() const {
  if (!last_redirect_end_time_ || !allow_redirect_details_)
    return 0.0;

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, last_redirect_end_time_, allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::fetchStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return PerformanceEntry::startTime();

  if (last_redirect_end_time_) {
    return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
        time_origin_, timing->RequestTime(), allow_negative_value_);
  }

  if (DOMHighResTimeStamp worker_ready_time = WorkerReady())
    return worker_ready_time;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->DnsStart() == 0.0)
    return fetchStart();

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->DnsStart(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupEnd() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->DnsEnd() == 0.0)
    return domainLookupStart();

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->DnsEnd(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::connectStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  // connectStart will be zero when a network request is not made.
  if (!timing || timing->ConnectStart() == 0.0 || DidReuseConnection())
    return domainLookupEnd();

  // connectStart includes any DNS time, so we may need to trim that off.
  double connect_start = timing->ConnectStart();
  if (timing->DnsEnd() > 0.0)
    connect_start = timing->DnsEnd();

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, connect_start, allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::connectEnd() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  // connectStart will be zero when a network request is not made.
  if (!timing || timing->ConnectEnd() == 0.0 || DidReuseConnection())
    return connectStart();

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->ConnectEnd(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::secureConnectionStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing ||
      timing->SslStart() == 0.0)  // Secure connection not negotiated.
    return 0.0;

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->SslStart(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::requestStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return connectEnd();

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->SendStart(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::responseStart() const {
  if (!AllowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return requestStart();

  // FIXME: This number isn't exactly correct. See the notes in
  // PerformanceTiming::responseStart().
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timing->ReceiveHeadersEnd(), allow_negative_value_);
}

DOMHighResTimeStamp PerformanceResourceTiming::responseEnd() const {
  if (!finish_time_)
    return responseStart();

  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, finish_time_, allow_negative_value_);
}

unsigned long long PerformanceResourceTiming::transferSize() const {
  if (!AllowTimingDetails())
    return 0;

  return GetTransferSize();
}

unsigned long long PerformanceResourceTiming::encodedBodySize() const {
  if (!AllowTimingDetails())
    return 0;

  return GetEncodedBodySize();
}

unsigned long long PerformanceResourceTiming::decodedBodySize() const {
  if (!AllowTimingDetails())
    return 0;

  return GetDecodedBodySize();
}

const HeapVector<Member<PerformanceServerTiming>>&
PerformanceResourceTiming::serverTiming() const {
  return server_timing_;
}

void PerformanceResourceTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("initiatorType", initiatorType());
  builder.AddString("nextHopProtocol", nextHopProtocol());
  builder.AddNumber("workerStart", workerStart());
  builder.AddNumber("redirectStart", redirectStart());
  builder.AddNumber("redirectEnd", redirectEnd());
  builder.AddNumber("fetchStart", fetchStart());
  builder.AddNumber("domainLookupStart", domainLookupStart());
  builder.AddNumber("domainLookupEnd", domainLookupEnd());
  builder.AddNumber("connectStart", connectStart());
  builder.AddNumber("connectEnd", connectEnd());
  builder.AddNumber("secureConnectionStart", secureConnectionStart());
  builder.AddNumber("requestStart", requestStart());
  builder.AddNumber("responseStart", responseStart());
  builder.AddNumber("responseEnd", responseEnd());
  builder.AddNumber("transferSize", transferSize());
  builder.AddNumber("encodedBodySize", encodedBodySize());
  builder.AddNumber("decodedBodySize", decodedBodySize());

  Vector<ScriptValue> server_timing;
  server_timing.ReserveCapacity(server_timing_.size());
  for (unsigned i = 0; i < server_timing_.size(); i++) {
    server_timing.push_back(
        server_timing_[i]->toJSONForBinding(builder.GetScriptState()));
  }
  builder.Add("serverTiming", server_timing);
}

void PerformanceResourceTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(server_timing_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
