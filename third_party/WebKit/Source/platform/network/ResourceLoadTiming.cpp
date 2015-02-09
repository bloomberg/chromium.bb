// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/network/ResourceLoadTiming.h"

#include "platform/TraceEvent.h"

namespace blink {

PassRefPtr<ResourceLoadTiming> ResourceLoadTiming::create()
{
    return adoptRef(new ResourceLoadTiming);
}

PassRefPtr<ResourceLoadTiming> ResourceLoadTiming::deepCopy()
{
    RefPtr<ResourceLoadTiming> timing = create();
    timing->m_requestTime = m_requestTime;
    timing->m_proxyStart = m_proxyStart;
    timing->m_proxyEnd = m_proxyEnd;
    timing->m_dnsStart = m_dnsStart;
    timing->m_dnsEnd = m_dnsEnd;
    timing->m_connectStart = m_connectStart;
    timing->m_connectEnd = m_connectEnd;
    timing->m_serviceWorkerFetchStart = m_serviceWorkerFetchStart;
    timing->m_serviceWorkerFetchReady = m_serviceWorkerFetchReady;
    timing->m_serviceWorkerFetchEnd = m_serviceWorkerFetchEnd;
    timing->m_sendStart = m_sendStart;
    timing->m_sendEnd = m_sendEnd;
    timing->m_receiveHeadersEnd = m_receiveHeadersEnd;
    timing->m_sslStart = m_sslStart;
    timing->m_sslEnd = m_sslEnd;
    return timing.release();
}

bool ResourceLoadTiming::operator==(const ResourceLoadTiming& other) const
{
    return m_requestTime == other.m_requestTime
        && m_proxyStart == other.m_proxyStart
        && m_proxyEnd == other.m_proxyEnd
        && m_dnsStart == other.m_dnsStart
        && m_dnsEnd == other.m_dnsEnd
        && m_connectStart == other.m_connectStart
        && m_connectEnd == other.m_connectEnd
        && m_serviceWorkerFetchStart == other.m_serviceWorkerFetchStart
        && m_serviceWorkerFetchReady == other.m_serviceWorkerFetchReady
        && m_serviceWorkerFetchEnd == other.m_serviceWorkerFetchEnd
        && m_sendStart == other.m_sendStart
        && m_sendEnd == other.m_sendEnd
        && m_receiveHeadersEnd == other.m_receiveHeadersEnd
        && m_sslStart == other.m_sslStart
        && m_sslEnd == other.m_sslEnd;
}

bool ResourceLoadTiming::operator!=(const ResourceLoadTiming& other) const
{
    return !(*this == other);
}

ResourceLoadTiming::ResourceLoadTiming()
    : m_requestTime(0)
    , m_proxyStart(0)
    , m_proxyEnd(0)
    , m_dnsStart(0)
    , m_dnsEnd(0)
    , m_connectStart(0)
    , m_connectEnd(0)
    , m_serviceWorkerFetchStart(0)
    , m_serviceWorkerFetchReady(0)
    , m_serviceWorkerFetchEnd(0)
    , m_sendStart(0)
    , m_sendEnd(0)
    , m_receiveHeadersEnd(0)
    , m_sslStart(0)
    , m_sslEnd(0)
{
}

void ResourceLoadTiming::setDnsStart(double dnsStart)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domainLookupStart", dnsStart);
    m_dnsStart = dnsStart;
}

void ResourceLoadTiming::setDnsEnd(double dnsEnd)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "domainLookupEnd", dnsEnd);
    m_dnsEnd = dnsEnd;
}

void ResourceLoadTiming::setConnectStart(double connectStart)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "connectStart", connectStart);
    m_connectStart = connectStart;
}

void ResourceLoadTiming::setConnectEnd(double connectEnd)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "connectEnd", connectEnd);
    m_connectEnd = connectEnd;
}

void ResourceLoadTiming::setSendStart(double sendStart)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "requestStart", sendStart);
    m_sendStart = sendStart;
}

void ResourceLoadTiming::setReceiveHeadersEnd(double receiveHeadersEnd)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "responseStart", receiveHeadersEnd);
    m_receiveHeadersEnd = receiveHeadersEnd;
}

void ResourceLoadTiming::setSslStart(double sslStart)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP("blink.user_timing", "secureConnectionStart", sslStart);
    m_sslStart = sslStart;
}

} // namespace blink
