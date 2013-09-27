/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/network/ResourceRequest.h"

#include "core/platform/network/ResourceRequest.h"

namespace WebCore {

double ResourceRequest::s_defaultTimeoutInterval = INT_MAX;

PassOwnPtr<ResourceRequest> ResourceRequest::adopt(PassOwnPtr<CrossThreadResourceRequestData> data)
{
    OwnPtr<ResourceRequest> request = adoptPtr(new ResourceRequest());
    request->setURL(data->m_url);
    request->setCachePolicy(data->m_cachePolicy);
    request->setTimeoutInterval(data->m_timeoutInterval);
    request->setFirstPartyForCookies(data->m_firstPartyForCookies);
    request->setHTTPMethod(data->m_httpMethod);
    request->setPriority(data->m_priority);

    request->m_httpHeaderFields.adopt(data->m_httpHeaders.release());

    request->setHTTPBody(data->m_httpBody);
    request->setAllowCookies(data->m_allowCookies);
    request->setHasUserGesture(data->m_hasUserGesture);
    request->setDownloadToFile(data->m_downloadToFile);
    request->setRequestorID(data->m_requestorID);
    request->setRequestorProcessID(data->m_requestorProcessID);
    request->setAppCacheHostID(data->m_appCacheHostID);
    request->setTargetType(data->m_targetType);
    return request.release();
}

PassOwnPtr<CrossThreadResourceRequestData> ResourceRequest::copyData() const
{
    OwnPtr<CrossThreadResourceRequestData> data = adoptPtr(new CrossThreadResourceRequestData());
    data->m_url = url().copy();
    data->m_cachePolicy = cachePolicy();
    data->m_timeoutInterval = timeoutInterval();
    data->m_firstPartyForCookies = firstPartyForCookies().copy();
    data->m_httpMethod = httpMethod().isolatedCopy();
    data->m_httpHeaders = httpHeaderFields().copyData();
    data->m_priority = priority();

    if (m_httpBody)
        data->m_httpBody = m_httpBody->deepCopy();
    data->m_allowCookies = m_allowCookies;
    data->m_hasUserGesture = m_hasUserGesture;
    data->m_downloadToFile = m_downloadToFile;
    data->m_requestorID = m_requestorID;
    data->m_requestorProcessID = m_requestorProcessID;
    data->m_appCacheHostID = m_appCacheHostID;
    data->m_targetType = m_targetType;
    return data.release();
}

bool ResourceRequest::isEmpty() const
{
    return m_url.isEmpty();
}

bool ResourceRequest::isNull() const
{
    return m_url.isNull();
}

const KURL& ResourceRequest::url() const
{
    return m_url;
}

void ResourceRequest::setURL(const KURL& url)
{
    m_url = url;
}

void ResourceRequest::removeCredentials()
{
    if (m_url.user().isEmpty() && m_url.pass().isEmpty())
        return;

    m_url.setUser(String());
    m_url.setPass(String());
}

ResourceRequestCachePolicy ResourceRequest::cachePolicy() const
{
    return m_cachePolicy;
}

void ResourceRequest::setCachePolicy(ResourceRequestCachePolicy cachePolicy)
{
    m_cachePolicy = cachePolicy;
}

double ResourceRequest::timeoutInterval() const
{
    return m_timeoutInterval;
}

void ResourceRequest::setTimeoutInterval(double timeoutInterval)
{
    m_timeoutInterval = timeoutInterval;
}

const KURL& ResourceRequest::firstPartyForCookies() const
{
    return m_firstPartyForCookies;
}

void ResourceRequest::setFirstPartyForCookies(const KURL& firstPartyForCookies)
{
    m_firstPartyForCookies = firstPartyForCookies;
}

const String& ResourceRequest::httpMethod() const
{
    return m_httpMethod;
}

void ResourceRequest::setHTTPMethod(const String& httpMethod)
{
    m_httpMethod = httpMethod;
}

const HTTPHeaderMap& ResourceRequest::httpHeaderFields() const
{
    return m_httpHeaderFields;
}

String ResourceRequest::httpHeaderField(const AtomicString& name) const
{
    return m_httpHeaderFields.get(name);
}

String ResourceRequest::httpHeaderField(const char* name) const
{
    return m_httpHeaderFields.get(name);
}

void ResourceRequest::setHTTPHeaderField(const AtomicString& name, const String& value)
{
    m_httpHeaderFields.set(name, value);
}

void ResourceRequest::setHTTPHeaderField(const char* name, const String& value)
{
    setHTTPHeaderField(AtomicString(name), value);
}

void ResourceRequest::clearHTTPAuthorization()
{
    m_httpHeaderFields.remove("Authorization");
}

void ResourceRequest::clearHTTPContentType()
{
    m_httpHeaderFields.remove("Content-Type");
}

void ResourceRequest::clearHTTPReferrer()
{
    m_httpHeaderFields.remove("Referer");
}

void ResourceRequest::clearHTTPOrigin()
{
    m_httpHeaderFields.remove("Origin");
}

void ResourceRequest::clearHTTPUserAgent()
{
    m_httpHeaderFields.remove("User-Agent");
}

void ResourceRequest::clearHTTPAccept()
{
    m_httpHeaderFields.remove("Accept");
}

FormData* ResourceRequest::httpBody() const
{
    return m_httpBody.get();
}

void ResourceRequest::setHTTPBody(PassRefPtr<FormData> httpBody)
{
    m_httpBody = httpBody;
}

bool ResourceRequest::allowCookies() const
{
    return m_allowCookies;
}

void ResourceRequest::setAllowCookies(bool allowCookies)
{
    m_allowCookies = allowCookies;
}

ResourceLoadPriority ResourceRequest::priority() const
{
    return m_priority;
}

void ResourceRequest::setPriority(ResourceLoadPriority priority)
{
    m_priority = priority;
}

void ResourceRequest::addHTTPHeaderField(const AtomicString& name, const String& value)
{
    HTTPHeaderMap::AddResult result = m_httpHeaderFields.add(name, value);
    if (!result.isNewEntry)
        result.iterator->value = result.iterator->value + ',' + value;
}

void ResourceRequest::addHTTPHeaderFields(const HTTPHeaderMap& headerFields)
{
    HTTPHeaderMap::const_iterator end = headerFields.end();
    for (HTTPHeaderMap::const_iterator it = headerFields.begin(); it != end; ++it)
        addHTTPHeaderField(it->key, it->value);
}

void ResourceRequest::clearHTTPHeaderField(const AtomicString& name)
{
    m_httpHeaderFields.remove(name);
}

bool equalIgnoringHeaderFields(const ResourceRequest& a, const ResourceRequest& b)
{
    if (a.url() != b.url())
        return false;

    if (a.cachePolicy() != b.cachePolicy())
        return false;

    if (a.timeoutInterval() != b.timeoutInterval())
        return false;

    if (a.firstPartyForCookies() != b.firstPartyForCookies())
        return false;

    if (a.httpMethod() != b.httpMethod())
        return false;

    if (a.allowCookies() != b.allowCookies())
        return false;

    if (a.priority() != b.priority())
        return false;

    FormData* formDataA = a.httpBody();
    FormData* formDataB = b.httpBody();

    if (!formDataA)
        return !formDataB;
    if (!formDataB)
        return !formDataA;

    if (*formDataA != *formDataB)
        return false;

    return true;
}

bool ResourceRequest::compare(const ResourceRequest& a, const ResourceRequest& b)
{
    if (!equalIgnoringHeaderFields(a, b))
        return false;

    if (a.httpHeaderFields() != b.httpHeaderFields())
        return false;

    return true;
}

bool ResourceRequest::isConditional() const
{
    return (m_httpHeaderFields.contains("If-Match") ||
            m_httpHeaderFields.contains("If-Modified-Since") ||
            m_httpHeaderFields.contains("If-None-Match") ||
            m_httpHeaderFields.contains("If-Range") ||
            m_httpHeaderFields.contains("If-Unmodified-Since"));
}

double ResourceRequest::defaultTimeoutInterval()
{
    return s_defaultTimeoutInterval;
}

void ResourceRequest::setDefaultTimeoutInterval(double timeoutInterval)
{
    s_defaultTimeoutInterval = timeoutInterval;
}

void ResourceRequest::initialize(const KURL& url, ResourceRequestCachePolicy cachePolicy)
{
    m_url = url;
    m_cachePolicy = cachePolicy;
    m_timeoutInterval = s_defaultTimeoutInterval;
    m_httpMethod = "GET";
    m_allowCookies = true;
    m_reportUploadProgress = false;
    m_reportLoadTiming = false;
    m_reportRawHeaders = false;
    m_hasUserGesture = false;
    m_downloadToFile = false;
    m_priority = ResourceLoadPriorityLow;
    m_requestorID = 0;
    m_requestorProcessID = 0;
    m_appCacheHostID = 0;
    m_targetType = TargetIsUnspecified;
}

// This is used by the loader to control the number of issued parallel load requests.
unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    // The chromium network stack already handles limiting the number of
    // parallel requests per host, so there's no need to do it here.  Therefore,
    // this is set to a high value that should never be hit in practice.
    return 10000;
}

}
