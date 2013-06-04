/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "core/loader/cache/CachedScript.h"

#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/loader/TextResourceDecoder.h"
#include "core/loader/cache/MemoryCache.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/SharedBuffer.h"
#include "core/platform/network/HTTPParsers.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedScript::CachedScript(const ResourceRequest& resourceRequest, const String& charset)
    : CachedResource(resourceRequest, Script)
    , m_decoder(TextResourceDecoder::create("application/javascript", charset))
{
    DEFINE_STATIC_LOCAL(const AtomicString, acceptScript, ("*/*", AtomicString::ConstructFromLiteral));

    // It's javascript we want.
    // But some websites think their scripts are <some wrong mimetype here>
    // and refuse to serve them if we only accept application/x-javascript.
    setAccept(acceptScript);
}

CachedScript::~CachedScript()
{
}

void CachedScript::setEncoding(const String& chs)
{
    m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedScript::encoding() const
{
    return m_decoder->encoding().name();
}

String CachedScript::mimeType() const
{
    return extractMIMETypeFromMediaType(m_response.httpHeaderField("Content-Type")).lower();
}

const String& CachedScript::script()
{
    ASSERT(!isPurgeable());
    ASSERT(isLoaded());

    if (!m_script && m_data) {
        m_script = m_decoder->decode(m_data->data(), encodedSize());
        m_script.append(m_decoder->flush());
        m_data.clear();
        // We lie a it here and claim that m_script counts as encoded data (even though it's really decoded data).
        // That's because the MemoryCache thinks that it can clear out decoded data by calling destroyDecodedData(),
        // but we can't destroy m_script in destroyDecodedData because that's our only copy of the data!
        setEncodedSize(m_script.sizeInBytes());
    }

    return m_script;
}

bool CachedScript::mimeTypeAllowedByNosniff() const
{
    return !parseContentTypeOptionsHeader(m_response.httpHeaderField("X-Content-Type-Options")) == ContentTypeOptionsNosniff || MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType());
}

void CachedScript::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CachedResourceScript);
    CachedResource::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_script, "script");
    info.addMember(m_decoder, "decoder");
}

} // namespace WebCore
