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

#include "weborigin/KnownPorts.h"

namespace WebCore {

void DOMURLUtils::setHref(DOMURLUtils* impl, const String& value)
{
    impl->setInput(value);
}

void DOMURLUtils::setProtocol(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (url.isNull())
        return;
    url.setProtocol(value);
    impl->setURL(url);
}

void DOMURLUtils::setUsername(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (url.isNull())
        return;
    url.setUser(value);
    impl->setURL(url);
}

void DOMURLUtils::setPassword(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (url.isNull())
        return;
    url.setPass(value);
    impl->setURL(url);
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

void DOMURLUtils::setPathname(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (!url.canSetPathname())
        return;
    url.setPath(value);
    impl->setURL(url);
}

void DOMURLUtils::setSearch(DOMURLUtils* impl, const String& value)
{
    KURL url = impl->url();
    if (!url.isValid())
        return;
    url.setQuery(value);
    impl->setURL(url);
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
