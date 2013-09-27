/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/fetch/FontResource.h"

#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceClientWalker.h"
#include "core/fetch/TextResourceDecoder.h"
#include "core/platform/HistogramSupport.h"
#include "platform/SharedBuffer.h"
#include "core/platform/graphics/FontCustomPlatformData.h"
#include "core/platform/graphics/FontPlatformData.h"
#include "wtf/CurrentTime.h"

#if ENABLE(SVG_FONTS)
#include "SVGNames.h"
#include "core/dom/NodeList.h"
#include "core/svg/SVGDocument.h"
#include "core/svg/SVGFontElement.h"
#endif

namespace WebCore {

FontResource::FontResource(const ResourceRequest& resourceRequest)
    : Resource(resourceRequest, Font)
    , m_loadInitiated(false)
{
}

FontResource::~FontResource()
{
}

void FontResource::load(ResourceFetcher*, const ResourceLoaderOptions& options)
{
    // Don't load the file yet. Wait for an access before triggering the load.
    setLoading(true);
    m_options = options;
}

void FontResource::didAddClient(ResourceClient* c)
{
    ASSERT(c->resourceClientType() == FontResourceClient::expectedType());
    Resource::didAddClient(c);
    if (!isLoading())
        static_cast<FontResourceClient*>(c)->fontLoaded(this);
}

void FontResource::beginLoadIfNeeded(ResourceFetcher* dl)
{
    if (!m_loadInitiated) {
        m_loadInitiated = true;
        Resource::load(dl, m_options);

        ResourceClientWalker<FontResourceClient> walker(m_clients);
        while (FontResourceClient* client = walker.next())
            client->didStartFontLoad(this);
        m_histograms.loadStarted();
    }
}

bool FontResource::ensureCustomFontData()
{
    if (!m_fontData && !errorOccurred() && !isLoading() && m_data) {
        m_fontData = FontCustomPlatformData::create(m_data.get());
        if (!m_fontData)
            setStatus(DecodeError);
    }
    return m_fontData;
}

FontPlatformData FontResource::platformDataFromCustomData(float size, bool bold, bool italic, FontOrientation orientation, FontWidthVariant widthVariant)
{
#if ENABLE(SVG_FONTS)
    if (m_externalSVGDocument)
        return FontPlatformData(size, bold, italic);
#endif
    ASSERT(m_fontData);
    return m_fontData->fontPlatformData(static_cast<int>(size), bold, italic, orientation, widthVariant);
}

#if ENABLE(SVG_FONTS)
bool FontResource::ensureSVGFontData()
{
    if (!m_externalSVGDocument && !errorOccurred() && !isLoading() && m_data) {
        m_externalSVGDocument = SVGDocument::create();

        RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("application/xml");
        String svgSource = decoder->decode(m_data->data(), m_data->size());
        svgSource.append(decoder->flush());

        m_externalSVGDocument->setContent(svgSource);

        if (decoder->sawError())
            m_externalSVGDocument = 0;
    }

    return m_externalSVGDocument;
}

SVGFontElement* FontResource::getSVGFontById(const String& fontName) const
{
    RefPtr<NodeList> list = m_externalSVGDocument->getElementsByTagNameNS(SVGNames::fontTag.namespaceURI(), SVGNames::fontTag.localName());
    if (!list)
        return 0;

    unsigned listLength = list->length();
    if (!listLength)
        return 0;

#ifndef NDEBUG
    for (unsigned i = 0; i < listLength; ++i) {
        ASSERT(list->item(i));
        ASSERT(list->item(i)->hasTagName(SVGNames::fontTag));
    }
#endif

    if (fontName.isEmpty())
        return toSVGFontElement(list->item(0));

    for (unsigned i = 0; i < listLength; ++i) {
        SVGFontElement* element = toSVGFontElement(list->item(i));
        if (element->getIdAttribute() == fontName)
            return element;
    }

    return 0;
}
#endif

void FontResource::allClientsRemoved()
{
    m_fontData.clear();
    Resource::allClientsRemoved();
}

void FontResource::checkNotify()
{
    ResourceClientWalker<FontResourceClient> w(m_clients);
    while (FontResourceClient* c = w.next())
        c->fontLoaded(this);
}

void FontResource::willUseFontData()
{
    if (!isLoaded())
        m_histograms.willUseFontData();
}

FontResource::FontResourceHistograms::~FontResourceHistograms()
{
    if (m_styledTime > 0)
        HistogramSupport::histogramEnumeration("WebFont.Resource.UsageType", StyledButNotUsed, UsageTypeMax);
}

void FontResource::FontResourceHistograms::willUseFontData()
{
    if (!m_styledTime)
        m_styledTime = currentTimeMS();
}

void FontResource::FontResourceHistograms::loadStarted()
{
    if (m_styledTime < 0)
        return;
    if (!m_styledTime) {
        HistogramSupport::histogramEnumeration("WebFont.Resource.UsageType", NotStyledButUsed, UsageTypeMax);
    } else {
        int duration = static_cast<int>(currentTimeMS() - m_styledTime);
        HistogramSupport::histogramCustomCounts("WebFont.Resource.StyleRecalcToDownloadLatency", duration, 0, 10000, 50);
        HistogramSupport::histogramEnumeration("WebFont.Resource.UsageType", StyledAndUsed, UsageTypeMax);
    }
    m_styledTime = -1;
}

}
