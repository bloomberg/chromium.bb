/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/page/Performance.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/MemoryInfo.h"
#include "core/page/PerformanceEntry.h"
#include "core/page/PerformanceNavigation.h"
#include "core/page/PerformanceResourceTiming.h"
#include "core/page/PerformanceTiming.h"
#include "core/page/PerformanceUserTiming.h"
#include "core/page/ResourceTimingInfo.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"

#include "core/page/Frame.h"

namespace WebCore {

static const size_t defaultResourceTimingBufferSize = 150;

Performance::Performance(Frame* frame)
    : DOMWindowProperty(frame)
    , m_resourceTimingBufferSize(defaultResourceTimingBufferSize)
    , m_userTiming(0)
    , m_referenceTime(frame->document()->loader()->timing()->referenceMonotonicTime())
{
    ASSERT(m_referenceTime);
    ScriptWrappable::init(this);
}

Performance::~Performance()
{
}

const AtomicString& Performance::interfaceName() const
{
    return eventNames().interfaceForPerformance;
}

ScriptExecutionContext* Performance::scriptExecutionContext() const
{
    if (!frame())
        return 0;
    return frame()->document();
}

PassRefPtr<MemoryInfo> Performance::memory() const
{
    return MemoryInfo::create(m_frame);
}

PerformanceNavigation* Performance::navigation() const
{
    if (!m_navigation)
        m_navigation = PerformanceNavigation::create(m_frame);

    return m_navigation.get();
}

PerformanceTiming* Performance::timing() const
{
    if (!m_timing)
        m_timing = PerformanceTiming::create(m_frame);

    return m_timing.get();
}

Vector<RefPtr<PerformanceEntry> > Performance::getEntries() const
{
    Vector<RefPtr<PerformanceEntry> > entries;

    entries.append(m_resourceTimingBuffer);

    if (m_userTiming) {
        entries.append(m_userTiming->getMarks());
        entries.append(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<RefPtr<PerformanceEntry> > Performance::getEntriesByType(const String& entryType)
{
    Vector<RefPtr<PerformanceEntry> > entries;

    if (equalIgnoringCase(entryType, "resource"))
        for (Vector<RefPtr<PerformanceEntry> >::const_iterator resource = m_resourceTimingBuffer.begin(); resource != m_resourceTimingBuffer.end(); ++resource)
            entries.append(*resource);

    if (m_userTiming) {
        if (equalIgnoringCase(entryType, "mark"))
            entries.append(m_userTiming->getMarks());
        else if (equalIgnoringCase(entryType, "measure"))
            entries.append(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<RefPtr<PerformanceEntry> > Performance::getEntriesByName(const String& name, const String& entryType)
{
    Vector<RefPtr<PerformanceEntry> > entries;

    if (entryType.isNull() || equalIgnoringCase(entryType, "resource"))
        for (Vector<RefPtr<PerformanceEntry> >::const_iterator resource = m_resourceTimingBuffer.begin(); resource != m_resourceTimingBuffer.end(); ++resource)
            if ((*resource)->name() == name)
                entries.append(*resource);

    if (m_userTiming) {
        if (entryType.isNull() || equalIgnoringCase(entryType, "mark"))
            entries.append(m_userTiming->getMarks(name));
        if (entryType.isNull() || equalIgnoringCase(entryType, "measure"))
            entries.append(m_userTiming->getMeasures(name));
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

void Performance::webkitClearResourceTimings()
{
    m_resourceTimingBuffer.clear();
}

void Performance::webkitSetResourceTimingBufferSize(unsigned size)
{
    m_resourceTimingBufferSize = size;
    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(eventNames().webkitresourcetimingbufferfullEvent, false, false));
}

static bool passesTimingAllowCheck(const ResourceResponse& response, Document* requestingDocument)
{
    AtomicallyInitializedStatic(AtomicString&, timingAllowOrigin = *new AtomicString("timing-allow-origin"));

    RefPtr<SecurityOrigin> resourceOrigin = SecurityOrigin::create(response.url());
    if (resourceOrigin->isSameSchemeHostPort(requestingDocument->securityOrigin()))
        return true;

    const String& timingAllowOriginString = response.httpHeaderField(timingAllowOrigin);
    if (timingAllowOriginString.isEmpty() || equalIgnoringCase(timingAllowOriginString, "null"))
        return false;

    if (timingAllowOriginString == "*")
        return true;

    const String& securityOrigin = requestingDocument->securityOrigin()->toString();
    Vector<String> timingAllowOrigins;
    timingAllowOriginString.split(" ", timingAllowOrigins);
    for (size_t i = 0; i < timingAllowOrigins.size(); ++i) {
        if (timingAllowOrigins[i] == securityOrigin)
            return true;
    }

    return false;
}

static bool allowsTimingRedirect(const Vector<ResourceResponse>& redirectChain, const ResourceResponse& finalResponse, Document* initiatorDocument)
{
    if (!passesTimingAllowCheck(finalResponse, initiatorDocument))
        return false;

    for (size_t i = 0; i < redirectChain.size(); i++) {
        if (!passesTimingAllowCheck(redirectChain[i], initiatorDocument))
            return false;
    }

    return true;
}

void Performance::addResourceTiming(const ResourceTimingInfo& info, Document* initiatorDocument)
{
    if (isResourceTimingBufferFull())
        return;

    const ResourceResponse& finalResponse = info.finalResponse();
    bool allowTimingDetails = passesTimingAllowCheck(finalResponse, initiatorDocument);
    double startTime = info.initialTime();

    if (info.redirectChain().isEmpty()) {
        RefPtr<PerformanceEntry> entry = PerformanceResourceTiming::create(info, initiatorDocument, startTime, allowTimingDetails);
        addResourceTimingBuffer(entry);
        return;
    }

    const Vector<ResourceResponse>& redirectChain = info.redirectChain();
    bool allowRedirectDetails = allowsTimingRedirect(redirectChain, finalResponse, initiatorDocument);

    if (!allowRedirectDetails) {
        ResourceLoadTiming* finalTiming = finalResponse.resourceLoadTiming();
        ASSERT(finalTiming);
        if (finalTiming)
            startTime = finalTiming->requestTime;
    }

    ResourceLoadTiming* lastRedirectTiming = redirectChain.last().resourceLoadTiming();
    ASSERT(lastRedirectTiming);
    double lastRedirectEndTime = lastRedirectTiming->receiveHeadersEnd;

    RefPtr<PerformanceEntry> entry = PerformanceResourceTiming::create(info, initiatorDocument, startTime, lastRedirectEndTime, allowTimingDetails, allowRedirectDetails);
    addResourceTimingBuffer(entry);
}

void Performance::addResourceTimingBuffer(PassRefPtr<PerformanceEntry> entry)
{
    m_resourceTimingBuffer.append(entry);

    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(eventNames().webkitresourcetimingbufferfullEvent, false, false));
}

bool Performance::isResourceTimingBufferFull()
{
    return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

EventTargetData* Performance::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* Performance::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void Performance::mark(const String& markName, ExceptionState& es)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->mark(markName, es);
}

void Performance::clearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMarks(markName);
}

void Performance::measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState& es)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->measure(measureName, startMark, endMark, es);
}

void Performance::clearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMeasures(measureName);
}

double Performance::now() const
{
    return 1000.0 * (monotonicallyIncreasingTime() - m_referenceTime);
}

} // namespace WebCore
