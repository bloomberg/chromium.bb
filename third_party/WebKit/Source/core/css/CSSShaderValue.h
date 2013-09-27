/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CSSShaderValue_h
#define CSSShaderValue_h

#include "core/css/CSSValue.h"

namespace WebCore {

class ResourceFetcher;
class KURL;
class StyleFetchedShader;
class StyleShader;

class CSSShaderValue : public CSSValue {
public:
    static PassRefPtr<CSSShaderValue> create(const String& url) { return adoptRef(new CSSShaderValue(url)); }
    ~CSSShaderValue();

    const String& format() const { return m_format; }
    void setFormat(const String& format) { m_format = format; }

    KURL completeURL(ResourceFetcher*) const;
    StyleFetchedShader* resource(ResourceFetcher*);
    StyleShader* cachedOrPendingShader();

    String customCssText() const;

    bool equals(const CSSShaderValue&) const;

private:
    CSSShaderValue(const String& url);

    String m_url;
    String m_format;
    RefPtr<StyleShader> m_shader;
    bool m_accessedShader;
};

DEFINE_CSS_VALUE_TYPE_CASTS(ShaderValue);

} // namespace WebCore


#endif // CSSShaderValue_h
