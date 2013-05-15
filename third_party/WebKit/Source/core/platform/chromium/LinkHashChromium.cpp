/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/KURL.h"
#include "core/platform/LinkHash.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include <googleurl/src/url_util.h>
#include <public/Platform.h>

namespace WebCore {

// Visited Links --------------------------------------------------------------

LinkHash visitedLinkHash(const UChar* url, unsigned length)
{
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;
    if (!url_util::Canonicalize(url, length, 0, &buffer, &parsed))
        return 0; // Invalid URLs are unvisited.
    return WebKit::Platform::current()->visitedLinkHash(buffer.data(), buffer.length());
}

LinkHash visitedLinkHash(const String& url)
{
    return visitedLinkHash(url.characters(), url.length());
}

LinkHash visitedLinkHash(const KURL& base, const AtomicString& attributeURL)
{
    // Resolve the relative URL using googleurl and pass the absolute URL up to
    // the embedder. We could create a GURL object from the base and resolve
    // the relative URL that way, but calling the lower-level functions
    // directly saves us the string allocation in most cases.
    url_canon::RawCanonOutput<2048> buffer;
    url_parse::Parsed parsed;

    StringUTF8Adaptor baseUTF8(base.string());
    const url_parse::Parsed& baseParsed = base.parsed();

    bool isValid = false;
    const String& relative = attributeURL.string();
    if (!relative.isNull() && relative.is8Bit()) {
        StringUTF8Adaptor relativeUTF8(relative);
        isValid = url_util::ResolveRelative(baseUTF8.data(), baseUTF8.length(), baseParsed, relativeUTF8.data(), relativeUTF8.length(), 0, &buffer, &parsed);
    } else
        isValid = url_util::ResolveRelative(baseUTF8.data(), baseUTF8.length(), baseParsed, relative.characters16(), relative.length(), 0, &buffer, &parsed);

    if (!isValid)
        return 0;
    return WebKit::Platform::current()->visitedLinkHash(buffer.data(), buffer.length());
}

} // namespace WebCore
