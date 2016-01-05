/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include "platform/Logging.h"
#include "platform/fonts/Character.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontFallbackIterator.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "platform/fonts/shaping/RunSegmenter.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/text/TextBreakIterator.h"
#include "wtf/Compiler.h"
#include "wtf/MathExtras.h"
#include "wtf/text/Unicode.h"

#include <algorithm>
#include <hb.h>
#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>

namespace blink {

template<typename T>
class HarfBuzzScopedPtr {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(HarfBuzzScopedPtr);
public:
    typedef void (*DestroyFunction)(T*);

    HarfBuzzScopedPtr(T* ptr, DestroyFunction destroy)
        : m_ptr(ptr)
        , m_destroy(destroy)
    {
        ASSERT(m_destroy);
    }
    ~HarfBuzzScopedPtr()
    {
        if (m_ptr)
            (*m_destroy)(m_ptr);
    }

    T* get() { return m_ptr; }
    void set(T* ptr) { m_ptr = ptr; }
private:
    T* m_ptr;
    DestroyFunction m_destroy;
};

static inline float harfBuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

static void normalizeCharacters(const TextRun& run, unsigned length, UChar* destination, unsigned* destinationLength)
{
    unsigned position = 0;
    bool error = false;
    const UChar* source;
    String stringFor8BitRun;
    if (run.is8Bit()) {
        stringFor8BitRun = String::make16BitFrom8BitSource(run.characters8(), run.length());
        source = stringFor8BitRun.characters16();
    } else {
        source = run.characters16();
    }

    *destinationLength = 0;
    while (position < length) {
        UChar32 character;
        U16_NEXT(source, position, length, character);
        // Don't normalize tabs as they are not treated as spaces for word-end.
        if (run.normalizeSpace() && Character::isNormalizedCanvasSpaceCharacter(character))
            character = spaceCharacter;
        else if (Character::treatAsSpace(character) && character != noBreakSpaceCharacter)
            character = spaceCharacter;
        else if (Character::treatAsZeroWidthSpaceInComplexScript(character))
            character = zeroWidthSpaceCharacter;
        else if (Character::isModifier(character))
            character = zeroWidthSpaceCharacter;

        U16_APPEND(destination, *destinationLength, length, character, error);
        ASSERT_UNUSED(error, !error);
    }
}

HarfBuzzShaper::HarfBuzzShaper(const Font* font, const TextRun& run)
    : Shaper(font, run)
    , m_normalizedBufferLength(0)
    , m_wordSpacingAdjustment(font->fontDescription().wordSpacing())
    , m_letterSpacing(font->fontDescription().letterSpacing())
    , m_expansionOpportunityCount(0)
{
    m_normalizedBuffer = adoptArrayPtr(new UChar[m_textRun.length() + 1]);
    normalizeCharacters(m_textRun, m_textRun.length(), m_normalizedBuffer.get(), &m_normalizedBufferLength);
    setExpansion(m_textRun.expansion());
    setFontFeatures();
}

float HarfBuzzShaper::nextExpansionPerOpportunity()
{
    if (!m_expansionOpportunityCount) {
        ASSERT_NOT_REACHED(); // failures indicate that the logic in HarfBuzzShaper does not match to the one in expansionOpportunityCount()
        return 0;
    }
    if (!--m_expansionOpportunityCount) {
        float remaining = m_expansion;
        m_expansion = 0;
        return remaining;
    }
    m_expansion -= m_expansionPerOpportunity;
    return m_expansionPerOpportunity;
}

// setPadding sets a number of pixels to be distributed across the TextRun.
// WebKit uses this to justify text.
void HarfBuzzShaper::setExpansion(float padding)
{
    m_expansion = padding;
    if (!m_expansion)
        return;

    // If we have padding to distribute, then we try to give an equal
    // amount to each expansion opportunity.
    bool isAfterExpansion = m_isAfterExpansion;
    m_expansionOpportunityCount = Character::expansionOpportunityCount(m_normalizedBuffer.get(), m_normalizedBufferLength, m_textRun.direction(), isAfterExpansion, m_textRun.textJustify());
    if (isAfterExpansion && !m_textRun.allowsTrailingExpansion()) {
        ASSERT(m_expansionOpportunityCount > 0);
        --m_expansionOpportunityCount;
    }

    if (m_expansionOpportunityCount)
        m_expansionPerOpportunity = m_expansion / m_expansionOpportunityCount;
    else
        m_expansionPerOpportunity = 0;
}

static inline hb_feature_t createFeature(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint32_t value = 0)
{
    return { HB_TAG(c1, c2, c3, c4), value, 0 /* start */, static_cast<unsigned>(-1) /* end */ };
}

void HarfBuzzShaper::setFontFeatures()
{
    const FontDescription& description = m_font->fontDescription();

    static hb_feature_t noKern = createFeature('k', 'e', 'r', 'n');
    static hb_feature_t noVkrn = createFeature('v', 'k', 'r', 'n');
    switch (description.kerning()) {
    case FontDescription::NormalKerning:
        // kern/vkrn are enabled by default
        break;
    case FontDescription::NoneKerning:
        m_features.append(description.isVerticalAnyUpright() ? noVkrn : noKern);
        break;
    case FontDescription::AutoKerning:
        break;
    }

    static hb_feature_t noClig = createFeature('c', 'l', 'i', 'g');
    static hb_feature_t noLiga = createFeature('l', 'i', 'g', 'a');
    switch (description.commonLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        m_features.append(noLiga);
        m_features.append(noClig);
        break;
    case FontDescription::EnabledLigaturesState:
        // liga and clig are on by default
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t dlig = createFeature('d', 'l', 'i', 'g', 1);
    switch (description.discretionaryLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        // dlig is off by default
        break;
    case FontDescription::EnabledLigaturesState:
        m_features.append(dlig);
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t hlig = createFeature('h', 'l', 'i', 'g', 1);
    switch (description.historicalLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        // hlig is off by default
        break;
    case FontDescription::EnabledLigaturesState:
        m_features.append(hlig);
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t noCalt = createFeature('c', 'a', 'l', 't');
    switch (description.contextualLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        m_features.append(noCalt);
        break;
    case FontDescription::EnabledLigaturesState:
        // calt is on by default
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }

    static hb_feature_t hwid = createFeature('h', 'w', 'i', 'd', 1);
    static hb_feature_t twid = createFeature('t', 'w', 'i', 'd', 1);
    static hb_feature_t qwid = createFeature('q', 'w', 'i', 'd', 1);
    switch (description.widthVariant()) {
    case HalfWidth:
        m_features.append(hwid);
        break;
    case ThirdWidth:
        m_features.append(twid);
        break;
    case QuarterWidth:
        m_features.append(qwid);
        break;
    case RegularWidth:
        break;
    }

    FontFeatureSettings* settings = description.featureSettings();
    if (!settings)
        return;

    unsigned numFeatures = settings->size();
    for (unsigned i = 0; i < numFeatures; ++i) {
        hb_feature_t feature;
        const AtomicString& tag = settings->at(i).tag();
        feature.tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
        feature.value = settings->at(i).value();
        feature.start = 0;
        feature.end = static_cast<unsigned>(-1);
        m_features.append(feature);
    }
}

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built
// without hb-icu. See http://crbug.com/356929
static inline hb_script_t ICUScriptToHBScript(UScriptCode script)
{
    if (UNLIKELY(script == USCRIPT_INVALID_CODE))
        return HB_SCRIPT_INVALID;

    return hb_script_from_string(uscript_getShortName(script), -1);
}

static inline hb_direction_t TextDirectionToHBDirection(TextDirection dir, FontOrientation orientation, const SimpleFontData* fontData)
{
    hb_direction_t harfBuzzDirection = isVerticalAnyUpright(orientation) && !fontData->isTextOrientationFallback() ? HB_DIRECTION_TTB : HB_DIRECTION_LTR;
    return dir == RTL ? HB_DIRECTION_REVERSE(harfBuzzDirection) : harfBuzzDirection;
}

static const uint16_t* toUint16(const UChar* src)
{
    // FIXME: This relies on undefined behavior however it works on the
    // current versions of all compilers we care about and avoids making
    // a copy of the string.
    static_assert(sizeof(UChar) == sizeof(uint16_t), "UChar should be the same size as uint16_t");
    return reinterpret_cast<const uint16_t*>(src);
}

static inline void addToHarfBuzzBufferInternal(hb_buffer_t* buffer,
    const FontDescription& fontDescription, const UChar* normalizedBuffer,
    unsigned normalizedBufferLength, unsigned startIndex, unsigned numCharacters)
{
    // TODO: Revisit whether we can always fill the hb_buffer_t with the
    // full run text, but only specify startIndex and numCharacters for the part
    // to be shaped. Then simplify/change the complicated index computations in
    // extractShapeResults().
    if (fontDescription.variant() == FontVariantSmallCaps) {
        String upperText = String(normalizedBuffer, normalizedBufferLength)
            .upper();
        // TextRun is 16 bit, therefore upperText is 16 bit, even after we call
        // makeUpper().
        ASSERT(!upperText.is8Bit());
        hb_buffer_add_utf16(buffer, toUint16(upperText.characters16()),
            normalizedBufferLength, startIndex, numCharacters);
    } else {
        hb_buffer_add_utf16(buffer, toUint16(normalizedBuffer),
            normalizedBufferLength, startIndex, numCharacters);
    }
}

inline bool HarfBuzzShaper::shapeRange(hb_buffer_t* harfBuzzBuffer,
    unsigned startIndex,
    unsigned numCharacters,
    const SimpleFontData* currentFont,
    unsigned currentFontRangeFrom,
    unsigned currentFontRangeTo,
    UScriptCode currentRunScript,
    hb_language_t language)
{
    const FontPlatformData* platformData = &(currentFont->platformData());
    HarfBuzzFace* face = platformData->harfBuzzFace();
    if (!face) {
        WTF_LOG_ERROR("Could not create HarfBuzzFace from FontPlatformData.");
        return false;
    }

    hb_buffer_set_language(harfBuzzBuffer, language);
    hb_buffer_set_script(harfBuzzBuffer, ICUScriptToHBScript(currentRunScript));
    hb_buffer_set_direction(harfBuzzBuffer, TextDirectionToHBDirection(m_textRun.direction(),
        m_font->fontDescription().orientation(), currentFont));

    addToHarfBuzzBufferInternal(harfBuzzBuffer,
        m_font->fontDescription(), m_normalizedBuffer.get(), m_normalizedBufferLength,
        startIndex, numCharacters);

    HarfBuzzScopedPtr<hb_font_t> harfBuzzFont(face->createFont(currentFontRangeFrom, currentFontRangeTo), hb_font_destroy);
    hb_shape(harfBuzzFont.get(), harfBuzzBuffer, m_features.isEmpty() ? 0 : m_features.data(), m_features.size());

    return true;
}

bool HarfBuzzShaper::extractShapeResults(hb_buffer_t* harfBuzzBuffer,
    ShapeResult* shapeResult,
    bool& fontCycleQueued, const HolesQueueItem& currentQueueItem,
    const SimpleFontData* currentFont,
    UScriptCode currentRunScript,
    bool isLastResort)
{
    enum ClusterResult {
        Shaped,
        NotDef,
        Unknown
    };
    ClusterResult currentClusterResult = Unknown;
    ClusterResult previousClusterResult = Unknown;
    unsigned previousCluster = 0;
    unsigned currentCluster = 0;

    // Find first notdef glyph in harfBuzzBuffer.
    unsigned numGlyphs = hb_buffer_get_length(harfBuzzBuffer);
    hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(harfBuzzBuffer, 0);

    unsigned lastChangePosition = 0;

    if (!numGlyphs) {
        WTF_LOG_ERROR("HarfBuzz returned empty glyph buffer after shaping.");
        return false;
    }

    for (unsigned glyphIndex = 0; glyphIndex <= numGlyphs; ++glyphIndex) {
        // Iterating by clusters, check for when the state switches from shaped
        // to non-shaped and vice versa. Taking into account the edge cases of
        // beginning of the run and end of the run.
        previousCluster = currentCluster;
        currentCluster = glyphInfo[glyphIndex].cluster;

        if (glyphIndex < numGlyphs) {
            // Still the same cluster, merge shaping status.
            if (previousCluster == currentCluster && glyphIndex != 0) {
                if (glyphInfo[glyphIndex].codepoint == 0) {
                    currentClusterResult = NotDef;
                } else {
                    // We can only call the current cluster fully shapped, if
                    // all characters that are part of it are shaped, so update
                    // currentClusterResult to Shaped only if the previous
                    // characters have been shaped, too.
                    currentClusterResult = currentClusterResult == Shaped ? Shaped : NotDef;
                }
                continue;
            }
            // We've moved to a new cluster.
            previousClusterResult = currentClusterResult;
            currentClusterResult = glyphInfo[glyphIndex].codepoint == 0 ? NotDef : Shaped;
        } else {
            // The code below operates on the "flanks"/changes between NotDef
            // and Shaped. In order to keep the code below from explictly
            // dealing with character indices and run end, we explicitly
            // terminate the cluster/run here by setting the result value to the
            // opposite of what it was, leading to atChange turning true.
            previousClusterResult = currentClusterResult;
            currentClusterResult = currentClusterResult == NotDef ? Shaped : NotDef;
        }

        bool atChange = (previousClusterResult != currentClusterResult) && previousClusterResult != Unknown;
        if (!atChange)
            continue;

        // Compute the range indices of consecutive shaped or .notdef glyphs.
        // Cluster information for RTL runs becomes reversed, e.g. character 0
        // has cluster index 5 in a run of 6 characters.
        unsigned numCharacters = 0;
        unsigned numGlyphsToInsert = 0;
        unsigned startIndex = 0;
        if (HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harfBuzzBuffer))) {
            startIndex = glyphInfo[lastChangePosition].cluster;
            if (glyphIndex == numGlyphs) {
                numCharacters = currentQueueItem.m_startIndex + currentQueueItem.m_numCharacters - glyphInfo[lastChangePosition].cluster;
                numGlyphsToInsert = numGlyphs - lastChangePosition;
            } else {
                numCharacters = glyphInfo[glyphIndex].cluster - glyphInfo[lastChangePosition].cluster;
                numGlyphsToInsert = glyphIndex - lastChangePosition;
            }
        } else {
            // Direction Backwards
            startIndex = glyphInfo[glyphIndex - 1].cluster;
            if (lastChangePosition == 0) {
                numCharacters = currentQueueItem.m_startIndex + currentQueueItem.m_numCharacters - glyphInfo[glyphIndex - 1].cluster;
            } else {
                numCharacters = glyphInfo[lastChangePosition - 1].cluster - glyphInfo[glyphIndex - 1].cluster;
            }
            numGlyphsToInsert = glyphIndex - lastChangePosition;
        }

        if (currentClusterResult == Shaped && !isLastResort) {
            // Now it's clear that we need to continue processing.
            if (!fontCycleQueued) {
                appendToHolesQueue(HolesQueueNextFont, 0, 0);
                fontCycleQueued = true;
            }

            // Here we need to put character positions.
            ASSERT(numCharacters);
            appendToHolesQueue(HolesQueueRange, startIndex, numCharacters);
        }

        // If numCharacters is 0, that means we hit a NotDef before shaping the
        // whole grapheme. We do not append it here. For the next glyph we
        // encounter, atChange will be true, and the characters corresponding to
        // the grapheme will be added to the TODO queue again, attempting to
        // shape the whole grapheme with the next font.
        // When we're getting here with the last resort font, we have no other
        // choice than adding boxes to the ShapeResult.
        if ((currentClusterResult == NotDef && numCharacters) || isLastResort) {
            // Here we need to specify glyph positions.
            OwnPtr<ShapeResult::RunInfo> run = adoptPtr(new ShapeResult::RunInfo(currentFont,
                TextDirectionToHBDirection(m_textRun.direction(),
                m_font->fontDescription().orientation(), currentFont),
                ICUScriptToHBScript(currentRunScript),
                startIndex,
                numGlyphsToInsert, numCharacters));
            insertRunIntoShapeResult(shapeResult, run.release(), lastChangePosition, numGlyphsToInsert, harfBuzzBuffer);
        }
        lastChangePosition = glyphIndex;
    }
    return true;
}

static inline const SimpleFontData* fontDataAdjustedForOrientation(const SimpleFontData* originalFont,
    FontOrientation runOrientation,
    OrientationIterator::RenderOrientation renderOrientation)
{
    if (!isVerticalBaseline(runOrientation))
        return originalFont;

    if (runOrientation == FontOrientation::VerticalRotated
        || (runOrientation == FontOrientation::VerticalMixed && renderOrientation == OrientationIterator::OrientationRotateSideways))
        return originalFont->verticalRightOrientationFontData().get();

    return originalFont;
}

bool HarfBuzzShaper::collectFallbackHintChars(Vector<UChar32>& hint, bool needsList)
{
    if (!m_holesQueue.size())
        return false;

    hint.clear();

    size_t numCharsAdded = 0;
    for (auto it = m_holesQueue.begin(); it != m_holesQueue.end(); ++it) {
        if (it->m_action == HolesQueueNextFont)
            break;

        UChar32 hintChar;
        RELEASE_ASSERT(it->m_startIndex + it->m_numCharacters <= m_normalizedBufferLength);
        UTF16TextIterator iterator(m_normalizedBuffer.get() + it->m_startIndex, it->m_numCharacters);
        while (iterator.consume(hintChar)) {
            hint.append(hintChar);
            numCharsAdded++;
            if (!needsList)
                break;
            iterator.advance();
        }
    }
    return numCharsAdded > 0;
}

void HarfBuzzShaper::appendToHolesQueue(HolesQueueItemAction action,
    unsigned startIndex,
    unsigned numCharacters)
{
    m_holesQueue.append(HolesQueueItem(action, startIndex, numCharacters));
}

PassRefPtr<ShapeResult> HarfBuzzShaper::shapeResult()
{
    RefPtr<ShapeResult> result = ShapeResult::create(m_font,
        m_normalizedBufferLength, m_textRun.direction());
    HarfBuzzScopedPtr<hb_buffer_t> harfBuzzBuffer(hb_buffer_create(), hb_buffer_destroy);

    const FontDescription& fontDescription = m_font->fontDescription();
    const String& localeString = fontDescription.locale();
    CString locale = localeString.latin1();
    const hb_language_t language = hb_language_from_string(locale.data(), locale.length());

    RunSegmenter::RunSegmenterRange segmentRange = {
        0,
        0,
        USCRIPT_INVALID_CODE,
        OrientationIterator::OrientationInvalid,
        SmallCapsIterator::SmallCapsSameCase };
    RunSegmenter runSegmenter(
        m_normalizedBuffer.get(),
        m_normalizedBufferLength,
        m_font->fontDescription().orientation(),
        fontDescription.variant());

    Vector<UChar32> fallbackCharsHint;

    // TODO: Check whether this treatAsZerowidthspace from the previous script
    // segmentation plays a role here, does the new scriptRuniterator handle that correctly?
    while (runSegmenter.consume(&segmentRange)) {
        RefPtr<FontFallbackIterator> fallbackIterator = m_font->createFontFallbackIterator();

        appendToHolesQueue(HolesQueueNextFont, 0, 0);
        appendToHolesQueue(HolesQueueRange, segmentRange.start, segmentRange.end - segmentRange.start);

        const SimpleFontData* currentFont = nullptr;
        unsigned currentFontRangeFrom = 0;
        unsigned currentFontRangeTo = 0;

        bool fontCycleQueued = false;
        while (m_holesQueue.size()) {
            HolesQueueItem currentQueueItem = m_holesQueue.takeFirst();

            if (currentQueueItem.m_action == HolesQueueNextFont) {
                // For now, we're building a character list with which we probe
                // for needed fonts depending on the declared unicode-range of a
                // segmented CSS font. Alternatively, we can build a fake font
                // for the shaper and check whether any glyphs were found, or
                // define a new API on the shaper which will give us coverage
                // information?
                if (!collectFallbackHintChars(fallbackCharsHint, fallbackIterator->needsHintList())) {
                    // Give up shaping since we cannot retrieve a font fallback
                    // font without a hintlist.
                    m_holesQueue.clear();
                    break;
                }

                FontDataRange nextFontDataRange = fallbackIterator->next(fallbackCharsHint);
                currentFont = nextFontDataRange.fontData().get();
                currentFontRangeFrom = nextFontDataRange.from();
                currentFontRangeTo = nextFontDataRange.to();
                if (!currentFont) {
                    ASSERT(!m_holesQueue.size());
                    break;
                }
                fontCycleQueued = false;
                continue;
            }

            // TODO crbug.com/522964: Only use smallCapsFontData when the font does not support true smcp.  The spec
            // says: "To match the surrounding text, a font may provide alternate glyphs for caseless characters when
            // these features are enabled but when a user agent simulates small capitals, it must not attempt to
            // simulate alternates for codepoints which are considered caseless."
            const SimpleFontData* smallcapsAdjustedFont = segmentRange.smallCapsBehavior == SmallCapsIterator::SmallCapsUppercaseNeeded
                ? currentFont->smallCapsFontData(fontDescription).get()
                : currentFont;

            // Compatibility with SimpleFontData approach of keeping a flag for overriding drawing direction.
            // TODO: crbug.com/506224 This should go away in favor of storing that information elsewhere, for example in
            // ShapeResult.
            const SimpleFontData* directionAndSmallCapsAdjustedFont = fontDataAdjustedForOrientation(smallcapsAdjustedFont,
                m_font->fontDescription().orientation(),
                segmentRange.renderOrientation);

            if (!shapeRange(harfBuzzBuffer.get(),
                currentQueueItem.m_startIndex,
                currentQueueItem.m_numCharacters,
                directionAndSmallCapsAdjustedFont,
                currentFontRangeFrom,
                currentFontRangeTo,
                segmentRange.script,
                language))
                WTF_LOG_ERROR("Shaping range failed.");

            if (!extractShapeResults(harfBuzzBuffer.get(),
                result.get(),
                fontCycleQueued,
                currentQueueItem,
                directionAndSmallCapsAdjustedFont,
                segmentRange.script,
                !fallbackIterator->hasNext()))
                WTF_LOG_ERROR("Shape result extraction failed.");

            hb_buffer_reset(harfBuzzBuffer.get());
        }
    }
    return result.release();
}

// TODO crbug.com/542701: This should be a method on ShapeResult.
void HarfBuzzShaper::insertRunIntoShapeResult(ShapeResult* result,
    PassOwnPtr<ShapeResult::RunInfo> runToInsert, unsigned startGlyph, unsigned numGlyphs,
    hb_buffer_t* harfBuzzBuffer)
{
    ASSERT(numGlyphs > 0);
    OwnPtr<ShapeResult::RunInfo> run(std::move(runToInsert));
    ASSERT(numGlyphs == run->m_glyphData.size());

    const SimpleFontData* currentFontData = run->m_fontData.get();
    const hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(harfBuzzBuffer, 0);
    const hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(harfBuzzBuffer, 0);
    const unsigned startCluster = HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harfBuzzBuffer))
        ? glyphInfos[startGlyph].cluster : glyphInfos[startGlyph + numGlyphs - 1].cluster;

    float totalAdvance = 0.0f;
    FloatPoint glyphOrigin;
    float offsetX, offsetY;
    float* directionOffset = m_font->fontDescription().isVerticalAnyUpright() ? &offsetY : &offsetX;
    bool hasVerticalOffsets = !HB_DIRECTION_IS_HORIZONTAL(run->m_direction);

    // HarfBuzz returns result in visual order, no need to flip for RTL.
    for (unsigned i = 0; i < numGlyphs; ++i) {
        bool runEnd = i + 1 == numGlyphs;
        uint16_t glyph = glyphInfos[startGlyph + i].codepoint;
        offsetX = harfBuzzPositionToFloat(glyphPositions[startGlyph + i].x_offset);
        offsetY = -harfBuzzPositionToFloat(glyphPositions[startGlyph + i].y_offset);

        // One out of x_advance and y_advance is zero, depending on
        // whether the buffer direction is horizontal or vertical.
        float advance = harfBuzzPositionToFloat(glyphPositions[startGlyph + i].x_advance - glyphPositions[startGlyph + i].y_advance);
        unsigned currentCharacterIndex = glyphInfos[startGlyph + i].cluster;
        RELEASE_ASSERT(m_normalizedBufferLength > currentCharacterIndex);
        bool isClusterEnd = runEnd || glyphInfos[startGlyph + i].cluster != glyphInfos[startGlyph + i + 1].cluster;
        float spacing = 0;

        // The characterIndex of one ShapeResult run is normalized to the run's
        // startIndex and length.  TODO crbug.com/542703: Consider changing that
        // and instead pass the whole run to hb_buffer_t each time.
        run->m_glyphData[i].characterIndex = glyphInfos[startGlyph + i].cluster - startCluster;

        if (isClusterEnd)
            spacing += adjustSpacing(run.get(), i, currentCharacterIndex, *directionOffset, totalAdvance);

        advance += spacing;
        if (m_textRun.rtl()) {
            // In RTL, spacing should be added to left side of glyphs.
            *directionOffset += spacing;
            if (!isClusterEnd)
                *directionOffset += m_letterSpacing;
        }

        run->setGlyphAndPositions(i, glyph, advance, offsetX, offsetY);
        totalAdvance += advance;
        hasVerticalOffsets |= (offsetY != 0);

        FloatRect glyphBounds = currentFontData->boundsForGlyph(glyph);
        glyphBounds.move(glyphOrigin.x(), glyphOrigin.y());
        result->m_glyphBoundingBox.unite(glyphBounds);
        glyphOrigin += FloatSize(advance + offsetX, offsetY);
    }
    run->m_width = std::max(0.0f, totalAdvance);
    result->m_width += run->m_width;
    result->m_numGlyphs += numGlyphs;
    ASSERT(result->m_numGlyphs >= numGlyphs); // no overflow
    result->m_hasVerticalOffsets |= hasVerticalOffsets;

    // The runs are stored in result->m_runs in visual order. For LTR, we place
    // the run to be inserted before the next run with a bigger character
    // start index. For RTL, we place the run before the next run with a lower
    // character index. Otherwise, for both directions, at the end.
    if (HB_DIRECTION_IS_FORWARD(run->m_direction)) {
        for (size_t pos = 0; pos < result->m_runs.size(); ++pos) {
            if (result->m_runs.at(pos)->m_startIndex > run->m_startIndex) {
                result->m_runs.insert(pos, run.release());
                break;
            }
        }
    } else {
        for (size_t pos = 0; pos < result->m_runs.size(); ++pos) {
            if (result->m_runs.at(pos)->m_startIndex < run->m_startIndex) {
                result->m_runs.insert(pos, run.release());
                break;
            }
        }
    }
    // If we didn't find an existing slot to place it, append.
    if (run) {
        result->m_runs.append(run.release());
    }
}

PassRefPtr<ShapeResult> ShapeResult::createForTabulationCharacters(const Font* font,
    const TextRun& textRun, float positionOffset, unsigned count)
{
    const SimpleFontData* fontData = font->primaryFont();
    OwnPtr<ShapeResult::RunInfo> run = adoptPtr(new ShapeResult::RunInfo(fontData,
        // Tab characters are always LTR or RTL, not TTB, even when isVerticalAnyUpright().
        textRun.rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR,
        HB_SCRIPT_COMMON, 0, count, count));
    float position = textRun.xPos() + positionOffset;
    float startPosition = position;
    for (unsigned i = 0; i < count; i++) {
        float advance = font->tabWidth(*fontData, textRun.tabSize(), position);
        run->m_glyphData[i].characterIndex = i;
        run->setGlyphAndPositions(i, fontData->spaceGlyph(), advance, 0, 0);
        position += advance;
    }
    run->m_width = position - startPosition;

    RefPtr<ShapeResult> result = ShapeResult::create(font, count, textRun.direction());
    result->m_width = run->m_width;
    result->m_numGlyphs = count;
    ASSERT(result->m_numGlyphs == count); // no overflow
    result->m_runs.append(run.release());
    return result.release();
}

float HarfBuzzShaper::adjustSpacing(ShapeResult::RunInfo* run, size_t glyphIndex, unsigned currentCharacterIndex, float& offset, float& totalAdvance)
{
    float spacing = 0;
    UChar32 character = m_normalizedBuffer[currentCharacterIndex];
    if (m_letterSpacing && !Character::treatAsZeroWidthSpace(character))
        spacing += m_letterSpacing;

    bool treatAsSpace = Character::treatAsSpace(character);
    if (treatAsSpace && (currentCharacterIndex || character == noBreakSpaceCharacter) && (character != '\t' || !m_textRun.allowTabs()))
        spacing += m_wordSpacingAdjustment;

    if (!m_expansionOpportunityCount)
        return spacing;

    if (treatAsSpace) {
        spacing += nextExpansionPerOpportunity();
        m_isAfterExpansion = true;
        return spacing;
    }

    if (m_textRun.textJustify() != TextJustify::TextJustifyAuto) {
        m_isAfterExpansion = false;
        return spacing;
    }

    // isCJKIdeographOrSymbol() has expansion opportunities both before and after each character.
    // http://www.w3.org/TR/jlreq/#line_adjustment
    if (U16_IS_LEAD(character) && currentCharacterIndex + 1 < m_normalizedBufferLength && U16_IS_TRAIL(m_normalizedBuffer[currentCharacterIndex + 1]))
        character = U16_GET_SUPPLEMENTARY(character, m_normalizedBuffer[currentCharacterIndex + 1]);
    if (!Character::isCJKIdeographOrSymbol(character)) {
        m_isAfterExpansion = false;
        return spacing;
    }

    if (!m_isAfterExpansion) {
        // Take the expansion opportunity before this ideograph.
        float expandBefore = nextExpansionPerOpportunity();
        if (expandBefore) {
            if (glyphIndex > 0) {
                run->addAdvance(glyphIndex - 1, expandBefore);
                totalAdvance += expandBefore;
            } else {
                offset += expandBefore;
                spacing += expandBefore;
            }
        }
        if (!m_expansionOpportunityCount)
            return spacing;
    }

    // Don't need to check m_textRun.allowsTrailingExpansion() since it's covered by !m_expansionOpportunityCount above
    spacing += nextExpansionPerOpportunity();
    m_isAfterExpansion = true;
    return spacing;
}


} // namespace blink
