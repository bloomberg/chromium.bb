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

#include "config.h"

#include "core/css/CSSShaderValue.h"

#include "FetchInitiatorTypeNames.h"
#include "core/css/CSSParser.h"
#include "core/dom/Document.h"
#include "core/loader/cache/FetchRequest.h"
#include "core/loader/cache/ResourceFetcher.h"
#include "core/rendering/style/StyleFetchedShader.h"
#include "core/rendering/style/StylePendingShader.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

CSSShaderValue::CSSShaderValue(const String& url)
    : CSSValue(CSSShaderClass)
    , m_url(url)
    , m_accessedShader(false)
{
}

CSSShaderValue::~CSSShaderValue()
{
}

KURL CSSShaderValue::completeURL(ResourceFetcher* loader) const
{
    return loader->document()->completeURL(m_url);
}

StyleFetchedShader* CSSShaderValue::resource(ResourceFetcher* loader)
{
    ASSERT(loader);

    if (!m_accessedShader) {
        m_accessedShader = true;

        FetchRequest request(ResourceRequest(completeURL(loader)), FetchInitiatorTypeNames::css);
        if (ResourcePtr<ShaderResource> resource = loader->requestShader(request))
            m_shader = StyleFetchedShader::create(resource.get());
    }

    return (m_shader && m_shader->isShaderResource()) ? static_cast<StyleFetchedShader*>(m_shader.get()) : 0;
}

StyleShader* CSSShaderValue::cachedOrPendingShader()
{
    if (!m_shader)
        m_shader = StylePendingShader::create(this);

    return m_shader.get();
}

String CSSShaderValue::customCssText() const
{
    StringBuilder result;
    result.appendLiteral("url(");
    result.append(quoteCSSURLIfNeeded(m_url));
    result.append(')');
    if (!m_format.isEmpty()) {
        result.appendLiteral(" format('");
        result.append(m_format);
        result.appendLiteral("')");
    }
    return result.toString();
}

bool CSSShaderValue::equals(const CSSShaderValue& other) const
{
    return m_url == other.m_url;
}

} // namespace WebCore

