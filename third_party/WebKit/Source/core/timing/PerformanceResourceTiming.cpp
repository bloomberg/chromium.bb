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
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/ResourceTimingInfo.h"

namespace blink {

PerformanceResourceTiming::PerformanceResourceTiming(
    const AtomicString& initiatorType,
    double timeOrigin,
    ResourceLoadTiming* timing,
    double lastRedirectEndTime,
    double finishTime,
    unsigned long long transferSize,
    unsigned long long encodedBodyLength,
    unsigned long long decodedBodyLength,
    bool didReuseConnection,
    bool allowTimingDetails,
    bool allowRedirectDetails,
    const String& name,
    const String& entryType,
    double startTime)
    : PerformanceEntry(
          name,
          entryType,
          PerformanceBase::monotonicTimeToDOMHighResTimeStamp(timeOrigin,
                                                              startTime),
          PerformanceBase::monotonicTimeToDOMHighResTimeStamp(timeOrigin,
                                                              finishTime)),
      m_initiatorType(initiatorType),
      m_timeOrigin(timeOrigin),
      m_timing(timing),
      m_lastRedirectEndTime(lastRedirectEndTime),
      m_finishTime(finishTime),
      m_transferSize(transferSize),
      m_encodedBodySize(encodedBodyLength),
      m_decodedBodySize(decodedBodyLength),
      m_didReuseConnection(didReuseConnection),
      m_allowTimingDetails(allowTimingDetails),
      m_allowRedirectDetails(allowRedirectDetails) {}

PerformanceResourceTiming::PerformanceResourceTiming(
    const ResourceTimingInfo& info,
    double timeOrigin,
    double startTime,
    double lastRedirectEndTime,
    bool allowTimingDetails,
    bool allowRedirectDetails)
    : PerformanceResourceTiming(info.initiatorType(),
                                timeOrigin,
                                info.finalResponse().resourceLoadTiming(),
                                lastRedirectEndTime,
                                info.loadFinishTime(),
                                info.transferSize(),
                                info.finalResponse().encodedBodyLength(),
                                info.finalResponse().decodedBodyLength(),
                                info.finalResponse().connectionReused(),
                                allowTimingDetails,
                                allowRedirectDetails,
                                info.initialURL().getString(),
                                "resource",
                                startTime) {}

PerformanceResourceTiming::~PerformanceResourceTiming() {}

AtomicString PerformanceResourceTiming::initiatorType() const {
  return m_initiatorType;
}

DOMHighResTimeStamp PerformanceResourceTiming::workerStart() const {
  if (!m_timing || m_timing->workerStart() == 0.0)
    return 0.0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->workerStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::workerReady() const {
  if (!m_timing || m_timing->workerReady() == 0.0)
    return 0.0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->workerReady());
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectStart() const {
  if (!m_lastRedirectEndTime || !m_allowRedirectDetails)
    return 0.0;

  if (DOMHighResTimeStamp workerReadyTime = workerReady())
    return workerReadyTime;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::redirectEnd() const {
  if (!m_lastRedirectEndTime || !m_allowRedirectDetails)
    return 0.0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_lastRedirectEndTime);
}

DOMHighResTimeStamp PerformanceResourceTiming::fetchStart() const {
  if (m_lastRedirectEndTime) {
    // FIXME: ASSERT(m_timing) should be in constructor once timeticks of
    // AppCache is exposed from chrome network stack, crbug/251100
    ASSERT(m_timing);
    return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
        m_timeOrigin, m_timing->requestTime());
  }

  if (DOMHighResTimeStamp workerReadyTime = workerReady())
    return workerReadyTime;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupStart() const {
  if (!m_allowTimingDetails)
    return 0.0;

  if (!m_timing || m_timing->dnsStart() == 0.0)
    return fetchStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->dnsStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupEnd() const {
  if (!m_allowTimingDetails)
    return 0.0;

  if (!m_timing || m_timing->dnsEnd() == 0.0)
    return domainLookupStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->dnsEnd());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectStart() const {
  if (!m_allowTimingDetails)
    return 0.0;

  // connectStart will be zero when a network request is not made.
  if (!m_timing || m_timing->connectStart() == 0.0 || m_didReuseConnection)
    return domainLookupEnd();

  // connectStart includes any DNS time, so we may need to trim that off.
  double connectStart = m_timing->connectStart();
  if (m_timing->dnsEnd() > 0.0)
    connectStart = m_timing->dnsEnd();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             connectStart);
}

DOMHighResTimeStamp PerformanceResourceTiming::connectEnd() const {
  if (!m_allowTimingDetails)
    return 0.0;

  // connectStart will be zero when a network request is not made.
  if (!m_timing || m_timing->connectEnd() == 0.0 || m_didReuseConnection)
    return connectStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->connectEnd());
}

DOMHighResTimeStamp PerformanceResourceTiming::secureConnectionStart() const {
  if (!m_allowTimingDetails)
    return 0.0;

  if (!m_timing ||
      m_timing->sslStart() == 0.0)  // Secure connection not negotiated.
    return 0.0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->sslStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::requestStart() const {
  if (!m_allowTimingDetails)
    return 0.0;

  if (!m_timing)
    return connectEnd();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->sendStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseStart() const {
  if (!m_allowTimingDetails)
    return 0.0;

  if (!m_timing)
    return requestStart();

  // FIXME: This number isn't exactly correct. See the notes in
  // PerformanceTiming::responseStart().
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_timing->receiveHeadersEnd());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseEnd() const {
  if (!m_finishTime)
    return responseStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_finishTime);
}

unsigned long long PerformanceResourceTiming::transferSize() const {
  if (!m_allowTimingDetails)
    return 0;

  return m_transferSize;
}

unsigned long long PerformanceResourceTiming::encodedBodySize() const {
  if (!m_allowTimingDetails)
    return 0;

  return m_encodedBodySize;
}

unsigned long long PerformanceResourceTiming::decodedBodySize() const {
  if (!m_allowTimingDetails)
    return 0;

  return m_decodedBodySize;
}

void PerformanceResourceTiming::buildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::buildJSONValue(builder);
  builder.addString("initiatorType", initiatorType());
  builder.addNumber("workerStart", workerStart());
  builder.addNumber("redirectStart", redirectStart());
  builder.addNumber("redirectEnd", redirectEnd());
  builder.addNumber("fetchStart", fetchStart());
  builder.addNumber("domainLookupStart", domainLookupStart());
  builder.addNumber("domainLookupEnd", domainLookupEnd());
  builder.addNumber("connectStart", connectStart());
  builder.addNumber("connectEnd", connectEnd());
  builder.addNumber("secureConnectionStart", secureConnectionStart());
  builder.addNumber("requestStart", requestStart());
  builder.addNumber("responseStart", responseStart());
  builder.addNumber("responseEnd", responseEnd());
  builder.addNumber("transferSize", transferSize());
  builder.addNumber("encodedBodySize", encodedBodySize());
  builder.addNumber("decodedBodySize", decodedBodySize());
}

}  // namespace blink
