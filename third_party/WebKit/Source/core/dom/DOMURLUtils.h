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

#ifndef DOMURLUtils_h
#define DOMURLUtils_h

#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class KURL;

class DOMURLUtils {
public:
    virtual KURL url() const = 0;
    virtual void setURL(const KURL&) = 0;

    virtual String input() const = 0;
    virtual void setInput(const String&) = 0;

    static String href(DOMURLUtils*);
    static void setHref(DOMURLUtils*, const String&);

    static String toString(DOMURLUtils*);

    static String origin(DOMURLUtils*);

    static String protocol(DOMURLUtils*);
    static void setProtocol(DOMURLUtils*, const String&);

    static String username(DOMURLUtils*);
    static void setUsername(DOMURLUtils*, const String&);

    static String password(DOMURLUtils*);
    static void setPassword(DOMURLUtils*, const String&);

    static String host(DOMURLUtils*);
    static void setHost(DOMURLUtils*, const String&);

    static String hostname(DOMURLUtils*);
    static void setHostname(DOMURLUtils*, const String&);

    static String port(DOMURLUtils*);
    static void setPort(DOMURLUtils*, const String&);

    static String pathname(DOMURLUtils*);
    static void setPathname(DOMURLUtils*, const String&);

    static String search(DOMURLUtils*);
    static void setSearch(DOMURLUtils*, const String&);

    static String hash(DOMURLUtils*);
    static void setHash(DOMURLUtils*, const String&);
};

} // namespace WebCore

#endif // DOMURLUtils_h
