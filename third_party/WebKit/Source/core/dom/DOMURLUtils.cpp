/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/DOMURLUtils.h"

#include "weborigin/KURL.h"
#include "weborigin/KnownPorts.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

String DOMURLUtils::href(DOMURLUtils* impl)
{
    const KURL& url = impl->url();
    if (url.isNull())
        return impl->input();
    return url.string();
}

void DOMURLUtils::setHref(DOMURLUtils* impl, const String& value)
{
    impl->setInput(value);
}

String DOMURLUtils::toString(DOMURLUtils* impl)
{
    return href(impl);
}

String DOMURLUtils::origin(DOMURLUtils* impl)
{
    const KURL& url = impl->url();
    if (url.isNull())
        return "";

    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
    return origin->toString();
}

String DOMURLUtils::protocol(DOMURLUtils* impl)
{
    return impl->url().protocol() + ":";
}

void DOMURLUtils::setProtocol(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (url.isNull())
        return;
    url.setProtocol(value);
    impl->setURL(url);
}

String DOMURLUtils::username(DOMURLUtils* impl)
{
    return impl->url().user();
}

void DOMURLUtils::setUsername(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (url.isNull())
        return;
    url.setUser(value);
    impl->setURL(url);
}

String DOMURLUtils::password(DOMURLUtils* impl)
{
    return impl->url().pass();
}

void DOMURLUtils::setPassword(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (url.isNull())
        return;
    url.setPass(value);
    impl->setURL(url);
}

String DOMURLUtils::host(DOMURLUtils* impl)
{
    const KURL& url = impl->url();
    if (url.hostEnd() == url.pathStart())
        return url.host();
    if (isDefaultPortForProtocol(url.port(), url.protocol()))
        return url.host();
    return url.host() + ":" + String::number(url.port());
}

// This function does not allow leading spaces before the port number.
static unsigned parsePortFromStringPosition(const String& value, unsigned portStart, unsigned& portEnd)
{
    portEnd = portStart;
    while (isASCIIDigit(value[portEnd]))
        ++portEnd;
    return value.substring(portStart, portEnd - portStart).toUInt();
}

void DOMURLUtils::setHost(DOMURLUtils* impl, const String& value)
{
    // Update once spec bug is resolved: https://www.w3.org/Bugs/Public/show_bug.cgi?id=23474
    if (value.isEmpty())
        return;

    KURL url = impl->url();
    if (!url.canSetHostOrPort())
        return;

    size_t separator = value.find(':');
    if (!separator)
        return;

    if (separator == kNotFound) {
        url.setHost(value);
    } else {
        unsigned portEnd;
        unsigned port = parsePortFromStringPosition(value, separator + 1, portEnd);
        if (!port) {
            // Update once spec bug is resolved: https://www.w3.org/Bugs/Public/show_bug.cgi?id=23463

            // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
            // specifically goes against RFC 3986 (p3.2) and
            // requires setting the port to "0" if it is set to empty string.
            url.setHostAndPort(value.substring(0, separator + 1) + "0");
        } else {
            if (isDefaultPortForProtocol(port, url.protocol()))
                url.setHostAndPort(value.substring(0, separator));
            else
                url.setHostAndPort(value.substring(0, portEnd));
        }
    }

    impl->setURL(url);
}

String DOMURLUtils::hostname(DOMURLUtils* impl)
{
    return impl->url().host();
}

void DOMURLUtils::setHostname(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (!url.canSetHostOrPort())
        return;

    // Before setting new value:
    // Remove all leading U+002F SOLIDUS ("/") characters.
    unsigned i = 0;
    unsigned hostLength = value.length();
    while (value[i] == '/')
        i++;

    if (i == hostLength)
        return;

    url.setHost(value.substring(i));

    impl->setURL(url);
}

String DOMURLUtils::port(DOMURLUtils* impl)
{
    const KURL& url = impl->url();
    if (url.hasPort())
        return String::number(url.port());

    return emptyString();
}

void DOMURLUtils::setPort(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (!url.canSetHostOrPort())
        return;

    // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
    // specifically goes against RFC 3986 (p3.2) and
    // requires setting the port to "0" if it is set to empty string.
    unsigned port = value.toUInt();
    if (isDefaultPortForProtocol(port, url.protocol()))
        url.removePort();
    else
        url.setPort(port);

    impl->setURL(url);
}

String DOMURLUtils::pathname(DOMURLUtils* impl)
{
    return impl->url().path();
}

void DOMURLUtils::setPathname(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (!url.canSetPathname())
        return;

    if (value[0] == '/')
        url.setPath(value);
    else
        url.setPath("/" + value);

    impl->setURL(url);
}

String DOMURLUtils::search(DOMURLUtils* impl)
{
    String query = impl->url().query();
    return query.isEmpty() ? emptyString() : "?" + query;
}

void DOMURLUtils::setSearch(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (!url.isValid())
        return;

    String newSearch = value[0] == '?' ? value.substring(1) : value;
    // Make sure that '#' in the query does not leak to the hash.
    url.setQuery(newSearch.replaceWithLiteral('#', "%23"));

    impl->setURL(url);
}

String DOMURLUtils::hash(DOMURLUtils* impl)
{
    String fragmentIdentifier = impl->url().fragmentIdentifier();
    if (fragmentIdentifier.isEmpty())
        return emptyString();
    return AtomicString(String("#" + fragmentIdentifier));
}

void DOMURLUtils::setHash(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (url.isNull())
        return;

    if (value[0] == '#')
        url.setFragmentIdentifier(value.substring(1));
    else
        url.setFragmentIdentifier(value);

    impl->setURL(url);
}

} // namespace WebCore
