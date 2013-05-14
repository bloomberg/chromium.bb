/*
 * Copyright (c) 2008, 2009, 2011 Google Inc. All rights reserved.
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

#ifndef KURLPrivate_h
#define KURLPrivate_h

#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"
#include <googleurl/src/url_canon.h>
#include <googleurl/src/url_parse.h>

namespace WebCore {

class KURL;

class KURLPrivate {
public:
    KURLPrivate();
    KURLPrivate(const CString& canonicalSpec, const url_parse::Parsed&, bool isValid);
    explicit KURLPrivate(WTF::HashTableDeletedValueType);
    explicit KURLPrivate(const KURLPrivate&);

    KURLPrivate& operator=(const KURLPrivate&);

    void init(const KURL& base, const String& relative, const WTF::TextEncoding* queryEncoding);

    bool protocolIs(const char*) const;

    // Does a deep copy to the given output object.
    void copyTo(KURLPrivate* dest) const;

    String componentString(const url_parse::Component&) const;

    typedef url_canon::Replacements<url_parse::UTF16Char> Replacements;
    void replaceComponents(const Replacements&);

    const String& string() const;

    bool m_isValid;
    bool m_protocolIsInHTTPFamily;
    url_parse::Parsed m_parsed;

    KURL* innerURL() const { return m_innerURL.get(); }

    void reportMemoryUsage(MemoryObjectInfo*) const;
    bool isSafeToSendToAnotherThread() const;

private:
    // Backend initializer. The query encoding parameters are optional and can
    // be 0 (this implies UTF-8). This initializer requires that the object
    // has just been created and the strings are null. Do not call on an
    // already-constructed object.
    template <typename CHAR>
    void init(const KURL& base, const CHAR* relative, int relativeLength, const WTF::TextEncoding* queryEncoding);

    void initInnerURL();
    void initProtocolIsInHTTPFamily();

    String m_string;
    OwnPtr<KURL> m_innerURL;
};

} // namespace WebCore

#endif
