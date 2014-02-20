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

#include "platform/weborigin/KnownPorts.h"

namespace WebCore {

void DOMURLUtils::setHref(DOMURLUtils& impl, const String& value)
{
    impl.setInput(value);
}

void DOMURLUtils::setProtocol(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
    if (url.isNull())
        return;
    url.setProtocol(value);
    impl.setURL(url);
}

void DOMURLUtils::setUsername(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
    if (url.isNull())
        return;
    url.setUser(value);
    impl.setURL(url);
}

void DOMURLUtils::setPassword(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
    if (url.isNull())
        return;
    url.setPass(value);
    impl.setURL(url);
}

void DOMURLUtils::setHost(DOMURLUtils& impl, const String& value)
{
    if (value.isEmpty())
        return;

    KURL url = impl.url();
    if (!url.canSetHostOrPort())
        return;

    url.setHostAndPort(value);
    impl.setURL(url);
}

void DOMURLUtils::setHostname(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
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

    impl.setURL(url);
}

void DOMURLUtils::setPort(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
    if (!url.canSetHostOrPort())
        return;

    url.setPort(value);
    impl.setURL(url);
}

void DOMURLUtils::setPathname(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
    if (!url.canSetPathname())
        return;
    url.setPath(value);
    impl.setURL(url);
}

void DOMURLUtils::setSearch(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
    if (!url.isValid())
        return;
    url.setQuery(value);
    impl.setURL(url);
}

void DOMURLUtils::setHash(DOMURLUtils& impl, const String& value)
{
    KURL url = impl.url();
    if (url.isNull())
        return;

    if (value[0] == '#')
        url.setFragmentIdentifier(value.substring(1));
    else
        url.setFragmentIdentifier(value);

    impl.setURL(url);
}

} // namespace WebCore
