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

namespace blink {

PerformanceResourceTiming::PerformanceResourceTiming(
    const ResourceTimingInfo& info,
    double timeOrigin,
    double startTime,
    double lastRedirectEndTime,
    bool allowTimingDetails,
    bool allowRedirectDetails)
    : PerformanceEntry(
          info.initialURL().getString(),
          "resource",
          PerformanceBase::monotonicTimeToDOMHighResTimeStamp(timeOrigin,
                                                              startTime),
          PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
              timeOrigin,
              info.loadFinishTime())),
      m_initiatorType(info.initiatorType()),
      m_timeOrigin(timeOrigin),
      m_timing(info.finalResponse().resourceLoadTiming()),
      m_lastRedirectEndTime(lastRedirectEndTime),
      m_finishTime(info.loadFinishTime()),
      m_transferSize(info.transferSize()),
      m_encodedBodySize(info.finalResponse().encodedBodyLength()),
      m_decodedBodySize(info.finalResponse().decodedBodyLength()),
      m_didReuseConnection(info.finalResponse().connectionReused()),
      m_allowTimingDetails(allowTimingDetails),
      m_allowRedirectDetails(allowRedirectDetails) {}

// This constructor is for PerformanceNavigationTiming.
PerformanceResourceTiming::PerformanceResourceTiming(const String& name,
                                                     const String& entryType,
                                                     double startTime,
                                                     double duration)
    : PerformanceEntry(name, entryType, startTime, duration) {}

PerformanceResourceTiming::~PerformanceResourceTiming() {}

ResourceLoadTiming* PerformanceResourceTiming::resourceLoadTiming() const {
  return m_timing.get();
}

bool PerformanceResourceTiming::allowTimingDetails() const {
  return m_allowTimingDetails;
}

bool PerformanceResourceTiming::didReuseConnection() const {
  return m_didReuseConnection;
}

unsigned long long PerformanceResourceTiming::getTransferSize() const {
  return m_transferSize;
}

unsigned long long PerformanceResourceTiming::getEncodedBodySize() const {
  return m_encodedBodySize;
}

unsigned long long PerformanceResourceTiming::getDecodedBodySize() const {
  return m_decodedBodySize;
}

AtomicString PerformanceResourceTiming::initiatorType() const {
  return m_initiatorType;
}

DOMHighResTimeStamp PerformanceResourceTiming::workerStart() const {
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing || timing->workerStart() == 0.0)
    return 0.0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->workerStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::workerReady() const {
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing || timing->workerReady() == 0.0)
    return 0.0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->workerReady());
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
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing)
    return PerformanceEntry::startTime();

  if (m_lastRedirectEndTime) {
    return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
        m_timeOrigin, timing->requestTime());
  }

  if (DOMHighResTimeStamp workerReadyTime = workerReady())
    return workerReadyTime;

  return PerformanceEntry::startTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupStart() const {
  if (!allowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing || timing->dnsStart() == 0.0)
    return fetchStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->dnsStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::domainLookupEnd() const {
  if (!allowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing || timing->dnsEnd() == 0.0)
    return domainLookupStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             timing->dnsEnd());
}

DOMHighResTimeStamp PerformanceResourceTiming::connectStart() const {
  if (!allowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = resourceLoadTiming();
  // connectStart will be zero when a network request is not made.
  if (!timing || timing->connectStart() == 0.0 || didReuseConnection())
    return domainLookupEnd();

  // connectStart includes any DNS time, so we may need to trim that off.
  double connectStart = timing->connectStart();
  if (timing->dnsEnd() > 0.0)
    connectStart = timing->dnsEnd();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             connectStart);
}

DOMHighResTimeStamp PerformanceResourceTiming::connectEnd() const {
  if (!allowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = resourceLoadTiming();
  // connectStart will be zero when a network request is not made.
  if (!timing || timing->connectEnd() == 0.0 || didReuseConnection())
    return connectStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->connectEnd());
}

DOMHighResTimeStamp PerformanceResourceTiming::secureConnectionStart() const {
  if (!allowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing ||
      timing->sslStart() == 0.0)  // Secure connection not negotiated.
    return 0.0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->sslStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::requestStart() const {
  if (!allowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing)
    return connectEnd();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->sendStart());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseStart() const {
  if (!allowTimingDetails())
    return 0.0;
  ResourceLoadTiming* timing = resourceLoadTiming();
  if (!timing)
    return requestStart();

  // FIXME: This number isn't exactly correct. See the notes in
  // PerformanceTiming::responseStart().
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->receiveHeadersEnd());
}

DOMHighResTimeStamp PerformanceResourceTiming::responseEnd() const {
  if (!m_finishTime)
    return responseStart();

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_finishTime);
}

unsigned long long PerformanceResourceTiming::transferSize() const {
  if (!allowTimingDetails())
    return 0;

  return getTransferSize();
}

unsigned long long PerformanceResourceTiming::encodedBodySize() const {
  if (!allowTimingDetails())
    return 0;

  return getEncodedBodySize();
}

unsigned long long PerformanceResourceTiming::decodedBodySize() const {
  if (!allowTimingDetails())
    return 0;

  return getDecodedBodySize();
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
