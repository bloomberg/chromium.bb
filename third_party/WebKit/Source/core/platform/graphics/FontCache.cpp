/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "core/platform/graphics/FontCache.h"

#include "FontFamilyNames.h"

#include "RuntimeEnabledFeatures.h"
#include "core/platform/graphics/Font.h"
#include "core/platform/graphics/FontDataCache.h"
#include "core/platform/graphics/FontFallbackList.h"
#include "core/platform/graphics/FontPlatformData.h"
#include "core/platform/graphics/opentype/OpenTypeVerticalData.h"
#include "platform/fonts/AlternateFontFamily.h"
#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSelector.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/fonts/TextRenderingMode.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

using namespace WTF;

namespace WebCore {

FontCache* fontCache()
{
    DEFINE_STATIC_LOCAL(FontCache, globalFontCache, ());
    return &globalFontCache;
}

#if !OS(WIN) || ENABLE(GDI_FONTS_ON_WINDOWS)
FontCache::FontCache()
    : m_purgePreventCount(0)
{
}
#endif // !OS(WIN) || ENABLE(GDI_FONTS_ON_WINDOWS)

typedef HashMap<FontCacheKey, OwnPtr<FontPlatformData>, FontCacheKeyHash, FontCacheKeyTraits> FontPlatformDataCache;

static FontPlatformDataCache* gFontPlatformDataCache = 0;

FontPlatformData* FontCache::addFontResourcePlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    FontCacheKey key = fontDescription.cacheKey(family);
    OwnPtr<FontPlatformData>& result = gFontPlatformDataCache->add(key, nullptr).iterator->value;
    if (!result)
        result = adoptPtr(createFontPlatformData(fontDescription, family, fontDescription.effectiveFontSize()));
    return result.get();
}

FontPlatformData* FontCache::getFontResourcePlatformData(const FontDescription& fontDescription,
    const AtomicString& passedFamilyName, bool checkingAlternateName)
{
#if OS(WIN) && ENABLE(OPENTYPE_VERTICAL)
    // Leading "@" in the font name enables Windows vertical flow flag for the font.
    // Because we do vertical flow by ourselves, we don't want to use the Windows feature.
    // IE disregards "@" regardless of the orientatoin, so we follow the behavior.
    const AtomicString& familyName = (passedFamilyName.isEmpty() || passedFamilyName[0] != '@') ?
        passedFamilyName : AtomicString(passedFamilyName.impl()->substring(1));
#else
    const AtomicString& familyName = passedFamilyName;
#endif

    if (!gFontPlatformDataCache) {
        gFontPlatformDataCache = new FontPlatformDataCache;
        platformInit();
    }

    FontPlatformData* result = addFontResourcePlatformData(fontDescription, familyName);
    if (result || checkingAlternateName)
        return result;

    // We were unable to find a font. We have a small set of fonts that we alias to other names,
    // e.g., Arial/Helvetica, Courier/Courier New, etc. Try looking up the font under the aliased name.
    const AtomicString& alternateName = alternateFamilyName(familyName);
    if (!alternateName.isEmpty()) {
        if (FontPlatformData* alternateFontPlatformData = addFontResourcePlatformData(fontDescription, alternateName)) {
            FontCacheKey key = fontDescription.cacheKey(familyName);
            result = new FontPlatformData(*alternateFontPlatformData);
            gFontPlatformDataCache->set(key, adoptPtr(result));
        }
    }

    return result;
}

#if ENABLE(OPENTYPE_VERTICAL)
typedef HashMap<FontCache::FontFileKey, RefPtr<OpenTypeVerticalData>, WTF::IntHash<FontCache::FontFileKey>, WTF::UnsignedWithZeroKeyHashTraits<FontCache::FontFileKey> > FontVerticalDataCache;

FontVerticalDataCache& fontVerticalDataCacheInstance()
{
    DEFINE_STATIC_LOCAL(FontVerticalDataCache, fontVerticalDataCache, ());
    return fontVerticalDataCache;
}

PassRefPtr<OpenTypeVerticalData> FontCache::getVerticalData(const FontFileKey& key, const FontPlatformData& platformData)
{
    FontVerticalDataCache& fontVerticalDataCache = fontVerticalDataCacheInstance();
    FontVerticalDataCache::iterator result = fontVerticalDataCache.find(key);
    if (result != fontVerticalDataCache.end())
        return result.get()->value;

    RefPtr<OpenTypeVerticalData> verticalData = OpenTypeVerticalData::create(platformData);
    if (!verticalData->isOpenType())
        verticalData.clear();
    fontVerticalDataCache.set(key, verticalData);
    return verticalData;
}
#endif

static FontDataCache* gFontDataCache = 0;

PassRefPtr<SimpleFontData> FontCache::getFontResourceData(const FontDescription& fontDescription, const AtomicString& family, bool checkingAlternateName, ShouldRetain shouldRetain)
{
    if (FontPlatformData* platformData = getFontResourcePlatformData(fontDescription, adjustFamilyNameToAvoidUnsupportedFonts(family), checkingAlternateName))
        return getFontResourceData(platformData, shouldRetain);

    return 0;
}

PassRefPtr<SimpleFontData> FontCache::getFontResourceData(const FontPlatformData* platformData, ShouldRetain shouldRetain)
{
    if (!gFontDataCache)
        gFontDataCache = new FontDataCache;

#if !ASSERT_DISABLED
    if (shouldRetain == DoNotRetain)
        ASSERT(m_purgePreventCount);
#endif

    return gFontDataCache->get(platformData, shouldRetain);
}

bool FontCache::isPlatformFontAvailable(const FontDescription& fontDescription, const AtomicString& family)
{
    bool checkingAlternateName = true;
    return getFontResourcePlatformData(fontDescription, family, checkingAlternateName);
}

SimpleFontData* FontCache::getNonRetainedLastResortFallbackFont(const FontDescription& fontDescription)
{
    return getLastResortFallbackFont(fontDescription, DoNotRetain).leakRef();
}

void FontCache::releaseFontData(const SimpleFontData* fontData)
{
    ASSERT(gFontDataCache);

    gFontDataCache->release(fontData);
}

static inline void purgePlatformFontDataCache()
{
    if (!gFontPlatformDataCache)
        return;

    Vector<FontCacheKey> keysToRemove;
    keysToRemove.reserveInitialCapacity(gFontPlatformDataCache->size());
    FontPlatformDataCache::iterator platformDataEnd = gFontPlatformDataCache->end();
    for (FontPlatformDataCache::iterator platformData = gFontPlatformDataCache->begin(); platformData != platformDataEnd; ++platformData) {
        if (platformData->value && !gFontDataCache->contains(platformData->value.get()))
            keysToRemove.append(platformData->key);
    }

    size_t keysToRemoveCount = keysToRemove.size();
    for (size_t i = 0; i < keysToRemoveCount; ++i)
        gFontPlatformDataCache->remove(keysToRemove[i]);
}

static inline void purgeFontVerticalDataCache()
{
#if ENABLE(OPENTYPE_VERTICAL)
    FontVerticalDataCache& fontVerticalDataCache = fontVerticalDataCacheInstance();
    if (!fontVerticalDataCache.isEmpty()) {
        // Mark & sweep unused verticalData
        FontVerticalDataCache::iterator verticalDataEnd = fontVerticalDataCache.end();
        for (FontVerticalDataCache::iterator verticalData = fontVerticalDataCache.begin(); verticalData != verticalDataEnd; ++verticalData) {
            if (verticalData->value)
                verticalData->value->setInFontCache(false);
        }

        gFontDataCache->markAllVerticalData();

        Vector<FontCache::FontFileKey> keysToRemove;
        keysToRemove.reserveInitialCapacity(fontVerticalDataCache.size());
        for (FontVerticalDataCache::iterator verticalData = fontVerticalDataCache.begin(); verticalData != verticalDataEnd; ++verticalData) {
            if (!verticalData->value || !verticalData->value->inFontCache())
                keysToRemove.append(verticalData->key);
        }
        for (size_t i = 0, count = keysToRemove.size(); i < count; ++i)
            fontVerticalDataCache.take(keysToRemove[i]);
    }
#endif
}

void FontCache::purge(PurgeSeverity PurgeSeverity)
{
    // We should never be forcing the purge while the FontCachePurgePreventer is in scope.
    ASSERT(!m_purgePreventCount || PurgeSeverity == PurgeIfNeeded);
    if (m_purgePreventCount)
        return;

    if (!gFontDataCache || !gFontDataCache->purge(PurgeSeverity))
        return;

    purgePlatformFontDataCache();
    purgeFontVerticalDataCache();
}

static HashSet<FontSelector*>* gClients;

void FontCache::addClient(FontSelector* client)
{
    if (!gClients)
        gClients = new HashSet<FontSelector*>;

    ASSERT(!gClients->contains(client));
    gClients->add(client);
}

void FontCache::removeClient(FontSelector* client)
{
    ASSERT(gClients);
    ASSERT(gClients->contains(client));

    gClients->remove(client);
}

static unsigned short gGeneration = 0;

unsigned short FontCache::generation()
{
    return gGeneration;
}

void FontCache::invalidate()
{
    if (!gClients) {
        ASSERT(!gFontPlatformDataCache);
        return;
    }

    if (gFontPlatformDataCache) {
        delete gFontPlatformDataCache;
        gFontPlatformDataCache = new FontPlatformDataCache;
    }

    gGeneration++;

    Vector<RefPtr<FontSelector> > clients;
    size_t numClients = gClients->size();
    clients.reserveInitialCapacity(numClients);
    HashSet<FontSelector*>::iterator end = gClients->end();
    for (HashSet<FontSelector*>::iterator it = gClients->begin(); it != end; ++it)
        clients.append(*it);

    ASSERT(numClients == clients.size());
    for (size_t i = 0; i < numClients; ++i)
        clients[i]->fontCacheInvalidated();

    purge(ForcePurge);
}

} // namespace WebCore
