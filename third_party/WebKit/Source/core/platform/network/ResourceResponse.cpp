/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "core/platform/network/ResourceResponse.h"

#include "core/platform/network/HTTPParsers.h"
#include "core/platform/network/ResourceResponse.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

static void parseCacheHeader(const String& header, Vector<pair<String, String> >& result);

ResourceResponse::ResourceResponse()
    : m_expectedContentLength(0)
    , m_httpStatusCode(0)
    , m_lastModifiedDate(0)
    , m_wasCached(false)
    , m_connectionID(0)
    , m_connectionReused(false)
    , m_isNull(true)
    , m_haveParsedCacheControlHeader(false)
    , m_haveParsedAgeHeader(false)
    , m_haveParsedDateHeader(false)
    , m_haveParsedExpiresHeader(false)
    , m_haveParsedLastModifiedHeader(false)
    , m_cacheControlContainsNoCache(false)
    , m_cacheControlContainsNoStore(false)
    , m_cacheControlContainsMustRevalidate(false)
    , m_cacheControlMaxAge(0.0)
    , m_age(0.0)
    , m_date(0.0)
    , m_expires(0.0)
    , m_lastModified(0.0)
    , m_httpVersion(Unknown)
    , m_appCacheID(0)
    , m_isMultipartPayload(false)
    , m_wasFetchedViaSPDY(false)
    , m_wasNpnNegotiated(false)
    , m_wasAlternateProtocolAvailable(false)
    , m_wasFetchedViaProxy(false)
    , m_responseTime(0)
    , m_remotePort(0)
{
}

ResourceResponse::ResourceResponse(const KURL& url, const AtomicString& mimeType, long long expectedLength, const AtomicString& textEncodingName, const String& filename)
    : m_url(url)
    , m_mimeType(mimeType)
    , m_expectedContentLength(expectedLength)
    , m_textEncodingName(textEncodingName)
    , m_suggestedFilename(filename)
    , m_httpStatusCode(0)
    , m_lastModifiedDate(0)
    , m_wasCached(false)
    , m_connectionID(0)
    , m_connectionReused(false)
    , m_isNull(false)
    , m_haveParsedCacheControlHeader(false)
    , m_haveParsedAgeHeader(false)
    , m_haveParsedDateHeader(false)
    , m_haveParsedExpiresHeader(false)
    , m_haveParsedLastModifiedHeader(false)
    , m_cacheControlContainsNoCache(false)
    , m_cacheControlContainsNoStore(false)
    , m_cacheControlContainsMustRevalidate(false)
    , m_cacheControlMaxAge(0.0)
    , m_age(0.0)
    , m_date(0.0)
    , m_expires(0.0)
    , m_lastModified(0.0)
    , m_httpVersion(Unknown)
    , m_appCacheID(0)
    , m_isMultipartPayload(false)
    , m_wasFetchedViaSPDY(false)
    , m_wasNpnNegotiated(false)
    , m_wasAlternateProtocolAvailable(false)
    , m_wasFetchedViaProxy(false)
    , m_responseTime(0)
    , m_remotePort(0)
{
}

PassOwnPtr<ResourceResponse> ResourceResponse::adopt(PassOwnPtr<CrossThreadResourceResponseData> data)
{
    OwnPtr<ResourceResponse> response = adoptPtr(new ResourceResponse);
    response->setURL(data->m_url);
    response->setMimeType(data->m_mimeType);
    response->setExpectedContentLength(data->m_expectedContentLength);
    response->setTextEncodingName(data->m_textEncodingName);
    response->setSuggestedFilename(data->m_suggestedFilename);

    response->setHTTPStatusCode(data->m_httpStatusCode);
    response->setHTTPStatusText(data->m_httpStatusText);

    response->m_httpHeaderFields.adopt(data->m_httpHeaders.release());
    response->setLastModifiedDate(data->m_lastModifiedDate);
    response->setResourceLoadTiming(data->m_resourceLoadTiming.release());
    response->m_securityInfo = data->m_securityInfo;
    response->m_httpVersion = data->m_httpVersion;
    response->m_appCacheID = data->m_appCacheID;
    response->m_appCacheManifestURL = data->m_appCacheManifestURL.copy();
    response->m_isMultipartPayload = data->m_isMultipartPayload;
    response->m_wasFetchedViaSPDY = data->m_wasFetchedViaSPDY;
    response->m_wasNpnNegotiated = data->m_wasNpnNegotiated;
    response->m_wasAlternateProtocolAvailable = data->m_wasAlternateProtocolAvailable;
    response->m_wasFetchedViaProxy = data->m_wasFetchedViaProxy;
    response->m_responseTime = data->m_responseTime;
    response->m_remoteIPAddress = data->m_remoteIPAddress;
    response->m_remotePort = data->m_remotePort;
    response->m_downloadedFilePath = data->m_downloadedFilePath;
    response->m_downloadedFileHandle = data->m_downloadedFileHandle;

    // Bug https://bugs.webkit.org/show_bug.cgi?id=60397 this doesn't support
    // whatever values may be present in the opaque m_extraData structure.

    return response.release();
}

PassOwnPtr<CrossThreadResourceResponseData> ResourceResponse::copyData() const
{
    OwnPtr<CrossThreadResourceResponseData> data = adoptPtr(new CrossThreadResourceResponseData);
    data->m_url = url().copy();
    data->m_mimeType = mimeType().string().isolatedCopy();
    data->m_expectedContentLength = expectedContentLength();
    data->m_textEncodingName = textEncodingName().string().isolatedCopy();
    data->m_suggestedFilename = suggestedFilename().isolatedCopy();
    data->m_httpStatusCode = httpStatusCode();
    data->m_httpStatusText = httpStatusText().string().isolatedCopy();
    data->m_httpHeaders = httpHeaderFields().copyData();
    data->m_lastModifiedDate = lastModifiedDate();
    if (m_resourceLoadTiming)
        data->m_resourceLoadTiming = m_resourceLoadTiming->deepCopy();
    data->m_securityInfo = CString(m_securityInfo.data(), m_securityInfo.length());
    data->m_httpVersion = m_httpVersion;
    data->m_appCacheID = m_appCacheID;
    data->m_appCacheManifestURL = m_appCacheManifestURL.copy();
    data->m_isMultipartPayload = m_isMultipartPayload;
    data->m_wasFetchedViaSPDY = m_wasFetchedViaSPDY;
    data->m_wasNpnNegotiated = m_wasNpnNegotiated;
    data->m_wasAlternateProtocolAvailable = m_wasAlternateProtocolAvailable;
    data->m_wasFetchedViaProxy = m_wasFetchedViaProxy;
    data->m_responseTime = m_responseTime;
    data->m_remoteIPAddress = m_remoteIPAddress.string().isolatedCopy();
    data->m_remotePort = m_remotePort;
    data->m_downloadedFilePath = m_downloadedFilePath.isolatedCopy();
    data->m_downloadedFileHandle = m_downloadedFileHandle;

    // Bug https://bugs.webkit.org/show_bug.cgi?id=60397 this doesn't support
    // whatever values may be present in the opaque m_extraData structure.

    return data.release();
}

bool ResourceResponse::isHTTP() const
{
    return m_url.protocolIsInHTTPFamily();
}

const KURL& ResourceResponse::url() const
{
    return m_url;
}

void ResourceResponse::setURL(const KURL& url)
{
    m_isNull = false;

    m_url = url;
}

const AtomicString& ResourceResponse::mimeType() const
{
    return m_mimeType;
}

void ResourceResponse::setMimeType(const AtomicString& mimeType)
{
    m_isNull = false;

    // FIXME: MIME type is determined by HTTP Content-Type header. We should update the header, so that it doesn't disagree with m_mimeType.
    m_mimeType = mimeType;
}

long long ResourceResponse::expectedContentLength() const
{
    return m_expectedContentLength;
}

void ResourceResponse::setExpectedContentLength(long long expectedContentLength)
{
    m_isNull = false;

    // FIXME: Content length is determined by HTTP Content-Length header. We should update the header, so that it doesn't disagree with m_expectedContentLength.
    m_expectedContentLength = expectedContentLength;
}

const AtomicString& ResourceResponse::textEncodingName() const
{
    return m_textEncodingName;
}

void ResourceResponse::setTextEncodingName(const AtomicString& encodingName)
{
    m_isNull = false;

    // FIXME: Text encoding is determined by HTTP Content-Type header. We should update the header, so that it doesn't disagree with m_textEncodingName.
    m_textEncodingName = encodingName;
}

// FIXME should compute this on the fly
const String& ResourceResponse::suggestedFilename() const
{
    return m_suggestedFilename;
}

void ResourceResponse::setSuggestedFilename(const String& suggestedName)
{
    m_isNull = false;

    // FIXME: Suggested file name is calculated based on other headers. There should not be a setter for it.
    m_suggestedFilename = suggestedName;
}

int ResourceResponse::httpStatusCode() const
{
    return m_httpStatusCode;
}

void ResourceResponse::setHTTPStatusCode(int statusCode)
{
    m_httpStatusCode = statusCode;
}

const AtomicString& ResourceResponse::httpStatusText() const
{
    return m_httpStatusText;
}

void ResourceResponse::setHTTPStatusText(const AtomicString& statusText)
{
    m_httpStatusText = statusText;
}

String ResourceResponse::httpHeaderField(const AtomicString& name) const
{
    return m_httpHeaderFields.get(name);
}

String ResourceResponse::httpHeaderField(const char* name) const
{
    return m_httpHeaderFields.get(name);
}

void ResourceResponse::updateHeaderParsedState(const AtomicString& name)
{
    DEFINE_STATIC_LOCAL(const AtomicString, ageHeader, ("age", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, cacheControlHeader, ("cache-control", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, dateHeader, ("date", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, expiresHeader, ("expires", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, lastModifiedHeader, ("last-modified", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, pragmaHeader, ("pragma", AtomicString::ConstructFromLiteral));

    if (equalIgnoringCase(name, ageHeader))
        m_haveParsedAgeHeader = false;
    else if (equalIgnoringCase(name, cacheControlHeader) || equalIgnoringCase(name, pragmaHeader))
        m_haveParsedCacheControlHeader = false;
    else if (equalIgnoringCase(name, dateHeader))
        m_haveParsedDateHeader = false;
    else if (equalIgnoringCase(name, expiresHeader))
        m_haveParsedExpiresHeader = false;
    else if (equalIgnoringCase(name, lastModifiedHeader))
        m_haveParsedLastModifiedHeader = false;
}

void ResourceResponse::setHTTPHeaderField(const AtomicString& name, const String& value)
{
    updateHeaderParsedState(name);

    m_httpHeaderFields.set(name, value);
}

void ResourceResponse::addHTTPHeaderField(const AtomicString& name, const String& value)
{
    updateHeaderParsedState(name);

    HTTPHeaderMap::AddResult result = m_httpHeaderFields.add(name, value);
    if (!result.isNewEntry)
        result.iterator->value = result.iterator->value + ", " + value;
}

const HTTPHeaderMap& ResourceResponse::httpHeaderFields() const
{
    return m_httpHeaderFields;
}

void ResourceResponse::parseCacheControlDirectives() const
{
    ASSERT(!m_haveParsedCacheControlHeader);

    m_haveParsedCacheControlHeader = true;

    m_cacheControlContainsMustRevalidate = false;
    m_cacheControlContainsNoCache = false;
    m_cacheControlMaxAge = std::numeric_limits<double>::quiet_NaN();

    DEFINE_STATIC_LOCAL(const AtomicString, cacheControlString, ("cache-control", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, noCacheDirective, ("no-cache", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, noStoreDirective, ("no-store", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, mustRevalidateDirective, ("must-revalidate", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, maxAgeDirective, ("max-age", AtomicString::ConstructFromLiteral));

    String cacheControlValue = m_httpHeaderFields.get(cacheControlString);
    if (!cacheControlValue.isEmpty()) {
        Vector<pair<String, String> > directives;
        parseCacheHeader(cacheControlValue, directives);

        size_t directivesSize = directives.size();
        for (size_t i = 0; i < directivesSize; ++i) {
            // RFC2616 14.9.1: A no-cache directive with a value is only meaningful for proxy caches.
            // It should be ignored by a browser level cache.
            if (equalIgnoringCase(directives[i].first, noCacheDirective) && directives[i].second.isEmpty())
                m_cacheControlContainsNoCache = true;
            else if (equalIgnoringCase(directives[i].first, noStoreDirective))
                m_cacheControlContainsNoStore = true;
            else if (equalIgnoringCase(directives[i].first, mustRevalidateDirective))
                m_cacheControlContainsMustRevalidate = true;
            else if (equalIgnoringCase(directives[i].first, maxAgeDirective)) {
                if (!std::isnan(m_cacheControlMaxAge)) {
                    // First max-age directive wins if there are multiple ones.
                    continue;
                }
                bool ok;
                double maxAge = directives[i].second.toDouble(&ok);
                if (ok)
                    m_cacheControlMaxAge = maxAge;
            }
        }
    }

    if (!m_cacheControlContainsNoCache) {
        // Handle Pragma: no-cache
        // This is deprecated and equivalent to Cache-control: no-cache
        // Don't bother tokenizing the value, it is not important
        DEFINE_STATIC_LOCAL(const AtomicString, pragmaHeader, ("pragma", AtomicString::ConstructFromLiteral));
        String pragmaValue = m_httpHeaderFields.get(pragmaHeader);

        m_cacheControlContainsNoCache = pragmaValue.lower().contains(noCacheDirective);
    }
}

bool ResourceResponse::cacheControlContainsNoCache() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlContainsNoCache;
}

bool ResourceResponse::cacheControlContainsNoStore() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlContainsNoStore;
}

bool ResourceResponse::cacheControlContainsMustRevalidate() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlContainsMustRevalidate;
}

bool ResourceResponse::hasCacheValidatorFields() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, lastModifiedHeader, ("last-modified", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, eTagHeader, ("etag", AtomicString::ConstructFromLiteral));
    return !m_httpHeaderFields.get(lastModifiedHeader).isEmpty() || !m_httpHeaderFields.get(eTagHeader).isEmpty();
}

double ResourceResponse::cacheControlMaxAge() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlMaxAge;
}

static double parseDateValueInHeader(const HTTPHeaderMap& headers, const AtomicString& headerName)
{
    String headerValue = headers.get(headerName);
    if (headerValue.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    // This handles all date formats required by RFC2616:
    // Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
    // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
    // Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
    double dateInMilliseconds = parseDate(headerValue);
    if (!std::isfinite(dateInMilliseconds))
        return std::numeric_limits<double>::quiet_NaN();
    return dateInMilliseconds / 1000;
}

double ResourceResponse::date() const
{
    if (!m_haveParsedDateHeader) {
        DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("date", AtomicString::ConstructFromLiteral));
        m_date = parseDateValueInHeader(m_httpHeaderFields, headerName);
        m_haveParsedDateHeader = true;
    }
    return m_date;
}

double ResourceResponse::age() const
{
    if (!m_haveParsedAgeHeader) {
        DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("age", AtomicString::ConstructFromLiteral));
        String headerValue = m_httpHeaderFields.get(headerName);
        bool ok;
        m_age = headerValue.toDouble(&ok);
        if (!ok)
            m_age = std::numeric_limits<double>::quiet_NaN();
        m_haveParsedAgeHeader = true;
    }
    return m_age;
}

double ResourceResponse::expires() const
{
    if (!m_haveParsedExpiresHeader) {
        DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("expires", AtomicString::ConstructFromLiteral));
        m_expires = parseDateValueInHeader(m_httpHeaderFields, headerName);
        m_haveParsedExpiresHeader = true;
    }
    return m_expires;
}

double ResourceResponse::lastModified() const
{
    if (!m_haveParsedLastModifiedHeader) {
        DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("last-modified", AtomicString::ConstructFromLiteral));
        m_lastModified = parseDateValueInHeader(m_httpHeaderFields, headerName);
        m_haveParsedLastModifiedHeader = true;
    }
    return m_lastModified;
}

bool ResourceResponse::isAttachment() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, headerName, ("content-disposition", AtomicString::ConstructFromLiteral));
    String value = m_httpHeaderFields.get(headerName);
    size_t loc = value.find(';');
    if (loc != kNotFound)
        value = value.left(loc);
    value = value.stripWhiteSpace();
    DEFINE_STATIC_LOCAL(const AtomicString, attachmentString, ("attachment", AtomicString::ConstructFromLiteral));
    return equalIgnoringCase(value, attachmentString);
}

void ResourceResponse::setLastModifiedDate(time_t lastModifiedDate)
{
    m_lastModifiedDate = lastModifiedDate;
}

time_t ResourceResponse::lastModifiedDate() const
{
    return m_lastModifiedDate;
}

bool ResourceResponse::wasCached() const
{
    return m_wasCached;
}

void ResourceResponse::setWasCached(bool value)
{
    m_wasCached = value;
}

bool ResourceResponse::connectionReused() const
{
    return m_connectionReused;
}

void ResourceResponse::setConnectionReused(bool connectionReused)
{
    m_connectionReused = connectionReused;
}

unsigned ResourceResponse::connectionID() const
{
    return m_connectionID;
}

void ResourceResponse::setConnectionID(unsigned connectionID)
{
    m_connectionID = connectionID;
}

ResourceLoadTiming* ResourceResponse::resourceLoadTiming() const
{
    return m_resourceLoadTiming.get();
}

void ResourceResponse::setResourceLoadTiming(PassRefPtr<ResourceLoadTiming> resourceLoadTiming)
{
    m_resourceLoadTiming = resourceLoadTiming;
}

PassRefPtr<ResourceLoadInfo> ResourceResponse::resourceLoadInfo() const
{
    return m_resourceLoadInfo.get();
}

void ResourceResponse::setResourceLoadInfo(PassRefPtr<ResourceLoadInfo> loadInfo)
{
    m_resourceLoadInfo = loadInfo;
}

void ResourceResponse::setDownloadedFilePath(const String& downloadedFilePath)
{
    m_downloadedFilePath = downloadedFilePath;
    if (m_downloadedFilePath.isEmpty()) {
        m_downloadedFileHandle.clear();
        return;
    }
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->appendFile(m_downloadedFilePath);
    blobData->detachFromCurrentThread();
    m_downloadedFileHandle = BlobDataHandle::create(blobData.release(), -1);
}

bool ResourceResponse::compare(const ResourceResponse& a, const ResourceResponse& b)
{
    if (a.isNull() != b.isNull())
        return false;
    if (a.url() != b.url())
        return false;
    if (a.mimeType() != b.mimeType())
        return false;
    if (a.expectedContentLength() != b.expectedContentLength())
        return false;
    if (a.textEncodingName() != b.textEncodingName())
        return false;
    if (a.suggestedFilename() != b.suggestedFilename())
        return false;
    if (a.httpStatusCode() != b.httpStatusCode())
        return false;
    if (a.httpStatusText() != b.httpStatusText())
        return false;
    if (a.httpHeaderFields() != b.httpHeaderFields())
        return false;
    if (a.resourceLoadTiming() && b.resourceLoadTiming() && *a.resourceLoadTiming() == *b.resourceLoadTiming())
        return true;
    if (a.resourceLoadTiming() != b.resourceLoadTiming())
        return false;
    return true;
}

static bool isCacheHeaderSeparator(UChar c)
{
    // See RFC 2616, Section 2.2
    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
    }
}

static bool isControlCharacter(UChar c)
{
    return c < ' ' || c == 127;
}

static inline String trimToNextSeparator(const String& str)
{
    return str.substring(0, str.find(isCacheHeaderSeparator));
}

static void parseCacheHeader(const String& header, Vector<pair<String, String> >& result)
{
    const String safeHeader = header.removeCharacters(isControlCharacter);
    unsigned max = safeHeader.length();
    for (unsigned pos = 0; pos < max; /* pos incremented in loop */) {
        size_t nextCommaPosition = safeHeader.find(',', pos);
        size_t nextEqualSignPosition = safeHeader.find('=', pos);
        if (nextEqualSignPosition != kNotFound && (nextEqualSignPosition < nextCommaPosition || nextCommaPosition == kNotFound)) {
            // Get directive name, parse right hand side of equal sign, then add to map
            String directive = trimToNextSeparator(safeHeader.substring(pos, nextEqualSignPosition - pos).stripWhiteSpace());
            pos += nextEqualSignPosition - pos + 1;

            String value = safeHeader.substring(pos, max - pos).stripWhiteSpace();
            if (value[0] == '"') {
                // The value is a quoted string
                size_t nextDoubleQuotePosition = value.find('"', 1);
                if (nextDoubleQuotePosition != kNotFound) {
                    // Store the value as a quoted string without quotes
                    result.append(pair<String, String>(directive, value.substring(1, nextDoubleQuotePosition - 1).stripWhiteSpace()));
                    pos += (safeHeader.find('"', pos) - pos) + nextDoubleQuotePosition + 1;
                    // Move past next comma, if there is one
                    size_t nextCommaPosition2 = safeHeader.find(',', pos);
                    if (nextCommaPosition2 != kNotFound)
                        pos += nextCommaPosition2 - pos + 1;
                    else
                        return; // Parse error if there is anything left with no comma
                } else {
                    // Parse error; just use the rest as the value
                    result.append(pair<String, String>(directive, trimToNextSeparator(value.substring(1, value.length() - 1).stripWhiteSpace())));
                    return;
                }
            } else {
                // The value is a token until the next comma
                size_t nextCommaPosition2 = value.find(',');
                if (nextCommaPosition2 != kNotFound) {
                    // The value is delimited by the next comma
                    result.append(pair<String, String>(directive, trimToNextSeparator(value.substring(0, nextCommaPosition2).stripWhiteSpace())));
                    pos += (safeHeader.find(',', pos) - pos) + 1;
                } else {
                    // The rest is the value; no change to value needed
                    result.append(pair<String, String>(directive, trimToNextSeparator(value)));
                    return;
                }
            }
        } else if (nextCommaPosition != kNotFound && (nextCommaPosition < nextEqualSignPosition || nextEqualSignPosition == kNotFound)) {
            // Add directive to map with empty string as value
            result.append(pair<String, String>(trimToNextSeparator(safeHeader.substring(pos, nextCommaPosition - pos).stripWhiteSpace()), ""));
            pos += nextCommaPosition - pos + 1;
        } else {
            // Add last directive to map with empty string as value
            result.append(pair<String, String>(trimToNextSeparator(safeHeader.substring(pos, max - pos).stripWhiteSpace()), ""));
            return;
        }
    }
}

}
