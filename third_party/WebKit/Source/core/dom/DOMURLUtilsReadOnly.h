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

#ifndef DOMURLUtilsReadOnly_h
#define DOMURLUtilsReadOnly_h

#include "platform/weborigin/KURL.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMURLUtilsReadOnly {
public:
    virtual KURL url() const = 0;
    virtual String input() const = 0;
    virtual ~DOMURLUtilsReadOnly() { };

    static String href(DOMURLUtilsReadOnly&);

    static String origin(const KURL&);
    static String origin(DOMURLUtilsReadOnly& impl) { return origin(impl.url()); }

    static String protocol(const KURL& url) { return url.protocol() + ":"; }
    static String protocol(DOMURLUtilsReadOnly& impl) { return protocol(impl.url()); }

    static String username(const KURL& url) { return url.user(); }
    static String username(DOMURLUtilsReadOnly& impl) { return username(impl.url()); }

    static String password(const KURL& url) { return url.pass(); }
    static String password(DOMURLUtilsReadOnly& impl) { return password(impl.url()); }

    static String host(const KURL&);
    static String host(DOMURLUtilsReadOnly& impl) { return host(impl.url()); }

    static String hostname(const KURL& url) { return url.host(); }
    static String hostname(DOMURLUtilsReadOnly& impl) { return hostname(impl.url()); }

    static String port(const KURL&);
    static String port(DOMURLUtilsReadOnly& impl) { return port(impl.url()); }

    static String pathname(const KURL& url) { return url.path(); }
    static String pathname(DOMURLUtilsReadOnly& impl) { return pathname(impl.url()); }

    static String search(const KURL&);
    static String search(DOMURLUtilsReadOnly& impl) { return search(impl.url()); }

    static String hash(const KURL&);
    static String hash(DOMURLUtilsReadOnly& impl) { return hash(impl.url()); }
};

} // namespace WebCore

#endif // DOMURLUtilsReadOnly_h
