/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "core/css/CSSFontSelector.h"

#include "FontFamilyNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Frame.h"
#include "core/page/Settings.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/SimpleFontData.h"
#include "wtf/text/AtomicString.h"

using namespace std;

namespace WebCore {

CSSFontSelector::CSSFontSelector(Document* document)
    : m_document(document)
    , m_beginLoadingTimer(this, &CSSFontSelector::beginLoadTimerFired)
    , m_version(0)
{
    // FIXME: An old comment used to say there was no need to hold a reference to m_document
    // because "we are guaranteed to be destroyed before the document". But there does not
    // seem to be any such guarantee.

    ASSERT(m_document);
    fontCache()->addClient(this);
}

CSSFontSelector::~CSSFontSelector()
{
    if (!m_document) {
        ASSERT(!m_beginLoadingTimer.isActive());
        ASSERT(m_fontsToBeginLoading.isEmpty());
        return;
    }

    m_beginLoadingTimer.stop();

    ResourceFetcher* fetcher = m_document->fetcher();
    for (size_t i = 0; i < m_fontsToBeginLoading.size(); ++i) {
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        fetcher->decrementRequestCount(m_fontsToBeginLoading[i].get());
    }

    m_fontsToBeginLoading.clear();

    m_document = 0;

    fontCache()->removeClient(this);
}

bool CSSFontSelector::isEmpty() const
{
    return m_fonts.isEmpty();
}

void CSSFontSelector::addFontFaceRule(const StyleRuleFontFace* fontFaceRule)
{
    RefPtr<FontFace> fontFace = FontFace::create(fontFaceRule);
    if (!fontFace || fontFace->family().isEmpty())
        return;

    unsigned traitsMask = fontFace->traitsMask();
    if (!traitsMask)
        return;

    RefPtr<CSSFontFace> cssFontFace = fontFace->createCSSFontFace(m_document);
    if (!cssFontFace || !cssFontFace->isValid())
        return;

    OwnPtr<HashMap<unsigned, RefPtr<CSSSegmentedFontFace> > >& familyFontFaces = m_fontFaces.add(fontFace->family(), nullptr).iterator->value;
    if (!familyFontFaces) {
        familyFontFaces = adoptPtr(new HashMap<unsigned, RefPtr<CSSSegmentedFontFace> >);

        ASSERT(!m_locallyInstalledFontFaces.contains(fontFace->family()));

        Vector<unsigned> locallyInstalledFontsTraitsMasks;
        fontCache()->getTraitsInFamily(fontFace->family(), locallyInstalledFontsTraitsMasks);
        if (unsigned numLocallyInstalledFaces = locallyInstalledFontsTraitsMasks.size()) {
            OwnPtr<Vector<RefPtr<CSSSegmentedFontFace> > > familyLocallyInstalledFaces = adoptPtr(new Vector<RefPtr<CSSSegmentedFontFace> >);

            for (unsigned i = 0; i < numLocallyInstalledFaces; ++i) {
                RefPtr<CSSFontFace> locallyInstalledFontFace = CSSFontFace::create(0);
                locallyInstalledFontFace->addSource(adoptPtr(new CSSFontFaceSource(fontFace->family())));
                ASSERT(locallyInstalledFontFace->isValid());

                RefPtr<CSSSegmentedFontFace> segmentedFontFace = CSSSegmentedFontFace::create(this, static_cast<FontTraitsMask>(locallyInstalledFontsTraitsMasks[i]), true);
                segmentedFontFace->appendFontFace(locallyInstalledFontFace.release());
                familyLocallyInstalledFaces->append(segmentedFontFace);
            }

            m_locallyInstalledFontFaces.set(fontFace->family(), familyLocallyInstalledFaces.release());
        }
    }

    RefPtr<CSSSegmentedFontFace>& segmentedFontFace = familyFontFaces->add(traitsMask, 0).iterator->value;
    if (!segmentedFontFace)
        segmentedFontFace = CSSSegmentedFontFace::create(this, static_cast<FontTraitsMask>(traitsMask), false);

    segmentedFontFace->appendFontFace(cssFontFace);

    ++m_version;
}

void CSSFontSelector::registerForInvalidationCallbacks(FontSelectorClient* client)
{
    m_clients.add(client);
}

void CSSFontSelector::unregisterForInvalidationCallbacks(FontSelectorClient* client)
{
    m_clients.remove(client);
}

void CSSFontSelector::dispatchInvalidationCallbacks()
{
    Vector<FontSelectorClient*> clients;
    copyToVector(m_clients, clients);
    for (size_t i = 0; i < clients.size(); ++i)
        clients[i]->fontsNeedUpdate(this);

    // FIXME: Make Document a FontSelectorClient so that it can simply register for invalidation callbacks.
    if (!m_document)
        return;
    if (StyleResolver* styleResolver = m_document->styleResolverIfExists())
        styleResolver->invalidateMatchedPropertiesCache();
    if (!m_document->renderer())
        return;
    m_document->setNeedsStyleRecalc();
}

void CSSFontSelector::fontLoaded()
{
    dispatchInvalidationCallbacks();
}

void CSSFontSelector::fontCacheInvalidated()
{
    dispatchInvalidationCallbacks();
}

static PassRefPtr<FontData> fontDataForGenericFamily(Document* document, const FontDescription& fontDescription, const AtomicString& familyName)
{
    if (!document || !document->frame())
        return 0;

    const Settings* settings = document->frame()->settings();
    if (!settings)
        return 0;

    AtomicString genericFamily;
    UScriptCode script = fontDescription.script();

    if (familyName == serifFamily)
         genericFamily = settings->serifFontFamily(script);
    else if (familyName == sansSerifFamily)
         genericFamily = settings->sansSerifFontFamily(script);
    else if (familyName == cursiveFamily)
         genericFamily = settings->cursiveFontFamily(script);
    else if (familyName == fantasyFamily)
         genericFamily = settings->fantasyFontFamily(script);
    else if (familyName == monospaceFamily)
         genericFamily = settings->fixedFontFamily(script);
    else if (familyName == pictographFamily)
         genericFamily = settings->pictographFontFamily(script);
    else if (familyName == standardFamily)
         genericFamily = settings->standardFontFamily(script);

    if (!genericFamily.isEmpty())
        return fontCache()->getFontResourceData(fontDescription, genericFamily);

    return 0;
}

static inline bool compareFontFaces(CSSSegmentedFontFace* first, CSSSegmentedFontFace* second, FontTraitsMask desiredTraitsMask)
{
    FontTraitsMask firstTraitsMask = first->traitsMask();
    FontTraitsMask secondTraitsMask = second->traitsMask();

    bool firstHasDesiredVariant = firstTraitsMask & desiredTraitsMask & FontVariantMask;
    bool secondHasDesiredVariant = secondTraitsMask & desiredTraitsMask & FontVariantMask;

    if (firstHasDesiredVariant != secondHasDesiredVariant)
        return firstHasDesiredVariant;

    // We need to check font-variant css property for CSS2.1 compatibility.
    if ((desiredTraitsMask & FontVariantSmallCapsMask) && !first->isLocalFallback() && !second->isLocalFallback()) {
        // Prefer a font that has indicated that it can only support small-caps to a font that claims to support
        // all variants.  The specialized font is more likely to be true small-caps and not require synthesis.
        bool firstRequiresSmallCaps = (firstTraitsMask & FontVariantSmallCapsMask) && !(firstTraitsMask & FontVariantNormalMask);
        bool secondRequiresSmallCaps = (secondTraitsMask & FontVariantSmallCapsMask) && !(secondTraitsMask & FontVariantNormalMask);
        if (firstRequiresSmallCaps != secondRequiresSmallCaps)
            return firstRequiresSmallCaps;
    }

    bool firstHasDesiredStyle = firstTraitsMask & desiredTraitsMask & FontStyleMask;
    bool secondHasDesiredStyle = secondTraitsMask & desiredTraitsMask & FontStyleMask;

    if (firstHasDesiredStyle != secondHasDesiredStyle)
        return firstHasDesiredStyle;

    if ((desiredTraitsMask & FontStyleItalicMask) && !first->isLocalFallback() && !second->isLocalFallback()) {
        // Prefer a font that has indicated that it can only support italics to a font that claims to support
        // all styles.  The specialized font is more likely to be the one the author wants used.
        bool firstRequiresItalics = (firstTraitsMask & FontStyleItalicMask) && !(firstTraitsMask & FontStyleNormalMask);
        bool secondRequiresItalics = (secondTraitsMask & FontStyleItalicMask) && !(secondTraitsMask & FontStyleNormalMask);
        if (firstRequiresItalics != secondRequiresItalics)
            return firstRequiresItalics;
    }

    if (secondTraitsMask & desiredTraitsMask & FontWeightMask)
        return false;
    if (firstTraitsMask & desiredTraitsMask & FontWeightMask)
        return true;

    // http://www.w3.org/TR/2011/WD-css3-fonts-20111004/#font-matching-algorithm says :
    //   - If the desired weight is less than 400, weights below the desired weight are checked in descending order followed by weights above the desired weight in ascending order until a match is found.
    //   - If the desired weight is greater than 500, weights above the desired weight are checked in ascending order followed by weights below the desired weight in descending order until a match is found.
    //   - If the desired weight is 400, 500 is checked first and then the rule for desired weights less than 400 is used.
    //   - If the desired weight is 500, 400 is checked first and then the rule for desired weights less than 400 is used.

    static const unsigned fallbackRuleSets = 9;
    static const unsigned rulesPerSet = 8;
    static const FontTraitsMask weightFallbackRuleSets[fallbackRuleSets][rulesPerSet] = {
        { FontWeight200Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight100Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight200Mask, FontWeight100Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight500Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight700Mask, FontWeight800Mask, FontWeight900Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
        { FontWeight800Mask, FontWeight900Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
        { FontWeight900Mask, FontWeight700Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
        { FontWeight800Mask, FontWeight700Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask }
    };

    unsigned ruleSetIndex = 0;
    unsigned w = FontWeight100Bit;
    while (!(desiredTraitsMask & (1 << w))) {
        w++;
        ruleSetIndex++;
    }

    ASSERT(ruleSetIndex < fallbackRuleSets);
    const FontTraitsMask* weightFallbackRule = weightFallbackRuleSets[ruleSetIndex];
    for (unsigned i = 0; i < rulesPerSet; ++i) {
        if (secondTraitsMask & weightFallbackRule[i])
            return false;
        if (firstTraitsMask & weightFallbackRule[i])
            return true;
    }

    return false;
}

PassRefPtr<FontData> CSSFontSelector::getFontData(const FontDescription& fontDescription, const AtomicString& familyName)
{
    if (m_fontFaces.isEmpty()) {
        if (familyName.startsWith("-webkit-"))
            return fontDataForGenericFamily(m_document, fontDescription, familyName);
        if (fontDescription.genericFamily() == FontDescription::StandardFamily && !fontDescription.isSpecifiedFont())
            return fontDataForGenericFamily(m_document, fontDescription, "-webkit-standard");
        return 0;
    }

    CSSSegmentedFontFace* face = getFontFace(fontDescription, familyName);
    // If no face was found, then return 0 and let the OS come up with its best match for the name.
    if (!face) {
        // If we were handed a generic family, but there was no match, go ahead and return the correct font based off our
        // settings.
        if (fontDescription.genericFamily() == FontDescription::StandardFamily && !fontDescription.isSpecifiedFont())
            return fontDataForGenericFamily(m_document, fontDescription, "-webkit-standard");
        return fontDataForGenericFamily(m_document, fontDescription, familyName);
    }

    // We have a face. Ask it for a font data. If it cannot produce one, it will fail, and the OS will take over.
    return face->getFontData(fontDescription);
}

CSSSegmentedFontFace* CSSFontSelector::getFontFace(const FontDescription& fontDescription, const AtomicString& family)
{
    HashMap<unsigned, RefPtr<CSSSegmentedFontFace> >* familyFontFaces = m_fontFaces.get(family);
    if (!familyFontFaces || familyFontFaces->isEmpty())
        return 0;

    OwnPtr<HashMap<unsigned, RefPtr<CSSSegmentedFontFace> > >& segmentedFontFaceCache = m_fonts.add(family, nullptr).iterator->value;
    if (!segmentedFontFaceCache)
        segmentedFontFaceCache = adoptPtr(new HashMap<unsigned, RefPtr<CSSSegmentedFontFace> >);

    FontTraitsMask traitsMask = fontDescription.traitsMask();

    RefPtr<CSSSegmentedFontFace>& face = segmentedFontFaceCache->add(traitsMask, 0).iterator->value;
    if (!face) {
        for (HashMap<unsigned, RefPtr<CSSSegmentedFontFace> >::const_iterator i = familyFontFaces->begin(); i != familyFontFaces->end(); ++i) {
            CSSSegmentedFontFace* candidate = i->value.get();
            unsigned candidateTraitsMask = candidate->traitsMask();
            if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                continue;
            if ((traitsMask & FontVariantNormalMask) && !(candidateTraitsMask & FontVariantNormalMask))
                continue;
#if ENABLE(SVG_FONTS)
            // For SVG Fonts that specify that they only support the "normal" variant, we will assume they are incapable
            // of small-caps synthesis and just ignore the font face as a candidate.
            if (candidate->hasSVGFontFaceSource() && (traitsMask & FontVariantSmallCapsMask) && !(candidateTraitsMask & FontVariantSmallCapsMask))
                continue;
#endif
            if (!face || compareFontFaces(candidate, face.get(), traitsMask))
                face = candidate;
        }

        if (Vector<RefPtr<CSSSegmentedFontFace> >* familyLocallyInstalledFontFaces = m_locallyInstalledFontFaces.get(family)) {
            unsigned numLocallyInstalledFontFaces = familyLocallyInstalledFontFaces->size();
            for (unsigned i = 0; i < numLocallyInstalledFontFaces; ++i) {
                CSSSegmentedFontFace* candidate = familyLocallyInstalledFontFaces->at(i).get();
                unsigned candidateTraitsMask = candidate->traitsMask();
                if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                    continue;
                if ((traitsMask & FontVariantNormalMask) && !(candidateTraitsMask & FontVariantNormalMask))
                    continue;
                if (!face || compareFontFaces(candidate, face.get(), traitsMask))
                    face = candidate;
            }
        }
    }
    return face.get();
}

void CSSFontSelector::willUseFontData(const FontDescription& fontDescription, const AtomicString& family)
{
    CSSSegmentedFontFace* face = getFontFace(fontDescription, family);
    if (face)
        face->willUseFontData(fontDescription);
}

void CSSFontSelector::beginLoadingFontSoon(FontResource* font)
{
    if (!m_document)
        return;

    m_fontsToBeginLoading.append(font);
    // Increment the request count now, in order to prevent didFinishLoad from being dispatched
    // after this font has been requested but before it began loading. Balanced by
    // decrementRequestCount() in beginLoadTimerFired() and in clearDocument().
    m_document->fetcher()->incrementRequestCount(font);
    m_beginLoadingTimer.startOneShot(0);
}

void CSSFontSelector::beginLoadTimerFired(Timer<WebCore::CSSFontSelector>*)
{
    Vector<ResourcePtr<FontResource> > fontsToBeginLoading;
    fontsToBeginLoading.swap(m_fontsToBeginLoading);

    // CSSFontSelector could get deleted via beginLoadIfNeeded() or loadDone() unless protected.
    RefPtr<CSSFontSelector> protect(this);

    ResourceFetcher* fetcher = m_document->fetcher();
    for (size_t i = 0; i < fontsToBeginLoading.size(); ++i) {
        fontsToBeginLoading[i]->beginLoadIfNeeded(fetcher);
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        fetcher->decrementRequestCount(fontsToBeginLoading[i].get());
    }
    // Ensure that if the request count reaches zero, the frame loader will know about it.
    fetcher->didLoadResource(0);
    // New font loads may be triggered by layout after the document load is complete but before we have dispatched
    // didFinishLoading for the frame. Make sure the delegate is always dispatched by checking explicitly.
    if (m_document && m_document->frame())
        m_document->frame()->loader()->checkLoadComplete();
}

}
