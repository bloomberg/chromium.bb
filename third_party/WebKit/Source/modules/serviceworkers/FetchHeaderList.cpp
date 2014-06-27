// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FetchHeaderList.h"

#include "core/fetch/CrossOriginAccessControl.h"
#include "core/xml/XMLHttpRequest.h"
#include "platform/network/HTTPParsers.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

PassRefPtr<FetchHeaderList> FetchHeaderList::create()
{
    return adoptRef(new FetchHeaderList());
}

FetchHeaderList::FetchHeaderList()
{
}

FetchHeaderList::~FetchHeaderList()
{
}

void FetchHeaderList::append(const String& name, const String& value)
{
    // "To append a name/value (|name|/|value|) pair to a header list (|list|),
    // append a new header whose name is |name| and value is |value|, to
    // |list|."
    m_headerList.append(adoptPtr(new Header(name, value)));
}

void FetchHeaderList::set(const String& name, const String& value)
{
    // "To set a name/value (|name|/|value|) pair in a header list (|list|), run
    // these steps:
    // 1. If there are any headers in |list| whose name is |name|, set the value
    //    of the first such header to |value| and remove the others.
    // 2. Otherwise, append a new header whose name is |name| and value is
    //    |value|, to |list|."
    for (size_t i = 0; i < m_headerList.size(); ++i) {
        if (m_headerList[i]->first == name) {
            m_headerList[i]->second = value;
            for (size_t j = i + 1; j < m_headerList.size(); ) {
                if (m_headerList[j]->first == name)
                    m_headerList.remove(j);
                else
                    ++j;
            }
            return;
        }
    }
    m_headerList.append(adoptPtr(new Header(name, value)));
}

size_t FetchHeaderList::size() const
{
    return m_headerList.size();
}

void FetchHeaderList::remove(const String& name)
{
    for (size_t i = 0; i < m_headerList.size(); ) {
        if (m_headerList[i]->first == name)
            m_headerList.remove(i);
        else
            ++i;
    }
}

bool FetchHeaderList::get(const String& name, String& result) const
{
    for (size_t i = 0; i < m_headerList.size(); ++i) {
        if (m_headerList[i]->first == name) {
            result = m_headerList[i]->second;
            return true;
        }
    }
    return false;
}

void FetchHeaderList::getAll(const String& name, Vector<String>& result) const
{
    result.clear();
    for (size_t i = 0; i < m_headerList.size(); ++i) {
        if (m_headerList[i]->first == name)
            result.append(m_headerList[i]->second);
    }
}

bool FetchHeaderList::has(const String& name) const
{
    for (size_t i = 0; i < m_headerList.size(); ++i) {
        if (m_headerList[i]->first == name)
            return true;
    }
    return false;
}

void FetchHeaderList::clearList()
{
    m_headerList.clear();
}

bool FetchHeaderList::isValidHeaderName(const String& name)
{
    // FIXME: According to the spec (http://fetch.spec.whatwg.org/) should we
    // accept all characters in the range 0x00 to 0x7F?
    return isValidHTTPToken(name);
}

bool FetchHeaderList::isValidHeaderValue(const String& value)
{
    // "A value is a byte sequence that matches the field-value token production
    // and contains no 0x0A or 0x0D bytes."
    return (value.find('\r') == kNotFound) && (value.find('\n') == kNotFound);
}

bool FetchHeaderList::isSimpleHeader(const String& name, const String& value)
{
    // "A simple header is a header whose name is either one of `Accept`,
    // `Accept-Language`, and `Content-Language`, or whose name is
    // `Content-Type` and value, once parsed, is one of
    // `application/x-www-form-urlencoded`, `multipart/form-data`, and
    // `text/plain`."
    if (equalIgnoringCase(name, "accept")
        || equalIgnoringCase(name, "accept-language")
        || equalIgnoringCase(name, "content-language"))
        return true;

    if (equalIgnoringCase(name, "content-type")) {
        AtomicString mimeType = extractMIMETypeFromMediaType(AtomicString(value));
        return equalIgnoringCase(mimeType, "application/x-www-form-urlencoded")
            || equalIgnoringCase(mimeType, "multipart/form-data")
            || equalIgnoringCase(mimeType, "text/plain");
    }

    return false;
}

bool FetchHeaderList::isForbiddenHeaderName(const String& name)
{
    // "A forbidden header name is a header names that is one of:
    //   `Accept-Charset`, `Accept-Encoding`, `Access-Control-Request-Headers`,
    //   `Access-Control-Request-Method`, `Connection`,
    //   `Content-Length, Cookie`, `Cookie2`, `Date`, `DNT`, `Expect`, `Host`,
    //   `Keep-Alive`, `Origin`, `Referer`, `TE`, `Trailer`,
    //   `Transfer-Encoding`, `Upgrade`, `User-Agent`, `Via`
    // or starts with `Proxy-` or `Sec-` (including when it is just `Proxy-` or
    // `Sec-`)."
    return !XMLHttpRequest::isAllowedHTTPHeader(name);
}

bool FetchHeaderList::isForbiddenResponseHeaderName(const String& name)
{
    // "A forbidden response header name is a header name that is one of:
    // `Set-Cookie`, `Set-Cookie2`"
    return equalIgnoringCase(name, "set-cookie") || equalIgnoringCase(name, "set-cookie2");
}

} // namespace WebCore
