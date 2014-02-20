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
#include "core/dom/DOMURLUtilsReadOnly.h"

#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

String DOMURLUtilsReadOnly::href(DOMURLUtilsReadOnly& impl)
{
    const KURL& url = impl.url();
    if (url.isNull())
        return impl.input();
    return url.string();
}

String DOMURLUtilsReadOnly::origin(const KURL& url)
{
    if (url.isNull())
        return "";
    return SecurityOrigin::create(url)->toString();
}

String DOMURLUtilsReadOnly::host(const KURL& url)
{
    if (url.hostEnd() == url.pathStart())
        return url.host();
    if (isDefaultPortForProtocol(url.port(), url.protocol()))
        return url.host();
    return url.host() + ":" + String::number(url.port());
}

String DOMURLUtilsReadOnly::port(const KURL& url)
{
    if (url.hasPort())
        return String::number(url.port());

    return emptyString();
}

String DOMURLUtilsReadOnly::search(const KURL& url)
{
    String query = url.query();
    return query.isEmpty() ? emptyString() : "?" + query;
}

String DOMURLUtilsReadOnly::hash(const KURL& url)
{
    String fragmentIdentifier = url.fragmentIdentifier();
    if (fragmentIdentifier.isEmpty())
        return emptyString();
    return AtomicString(String("#" + fragmentIdentifier));
}

} // namespace WebCore
