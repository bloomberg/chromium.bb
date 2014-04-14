/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/html/parser/HTMLSrcsetParser.h"

#include "RuntimeEnabledFeatures.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "platform/ParsingUtilities.h"

namespace WebCore {

static bool compareByScaleFactor(const ImageCandidate& first, const ImageCandidate& second)
{
    return first.scaleFactor() < second.scaleFactor();
}

template<typename CharType>
inline bool isComma(CharType character)
{
    return character == ',';
}

template<typename CharType>
static bool parseDescriptors(const CharType* descriptorsStart, const CharType* descriptorsEnd, DescriptorParsingResult& result)
{
    const CharType* position = descriptorsStart;
    bool isValid = false;
    bool isEmptyDescriptor = !(descriptorsEnd > descriptorsStart);
    while (position < descriptorsEnd) {
        // 13.1. Let descriptor list be the result of splitting unparsed descriptors on spaces.
        skipWhile<CharType, isHTMLSpace<CharType> >(position, descriptorsEnd);
        const CharType* currentDescriptorStart = position;
        skipWhile<CharType, isNotHTMLSpace<CharType> >(position, descriptorsEnd);
        const CharType* currentDescriptorEnd = position;

        ++position;
        ASSERT(currentDescriptorEnd > currentDescriptorStart);
        --currentDescriptorEnd;
        unsigned descriptorLength = currentDescriptorEnd - currentDescriptorStart;
        if (*currentDescriptorEnd == 'x') {
            if (result.foundDescriptor())
                return false;
            result.scaleFactor = charactersToFloat(currentDescriptorStart, descriptorLength, &isValid);
            if (!isValid || result.scaleFactor < 0)
                return false;
        } else if (RuntimeEnabledFeatures::pictureSizesEnabled() && *currentDescriptorEnd == 'w') {
            if (result.foundDescriptor())
                return false;
            result.resourceWidth = charactersToInt(currentDescriptorStart, descriptorLength, &isValid);
            if (!isValid || result.resourceWidth <= 0)
                return false;
        }
    }
    if (isEmptyDescriptor)
        result.scaleFactor = 1.0;
    return result.foundDescriptor();
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/embedded-content-1.html#processing-the-image-candidates
template<typename CharType>
static void parseImageCandidatesFromSrcsetAttribute(const String& attribute, const CharType* attributeStart, unsigned length, Vector<ImageCandidate>& imageCandidates)
{
    const CharType* position = attributeStart;
    const CharType* attributeEnd = position + length;

    while (position < attributeEnd) {
        DescriptorParsingResult result;
        // 4. Splitting loop: Skip whitespace.
        skipWhile<CharType, isHTMLSpace<CharType> >(position, attributeEnd);
        if (position == attributeEnd)
            break;
        const CharType* imageURLStart = position;

        // If The current candidate is either totally empty or only contains space, skipping.
        if (*position == ',') {
            ++position;
            continue;
        }

        // 5. Collect a sequence of characters that are not space characters, and let that be url.
        skipUntil<CharType, isHTMLSpace<CharType> >(position, attributeEnd);
        const CharType* imageURLEnd = position;

        if (position != attributeEnd && *(position - 1) == ',') {
            --imageURLEnd;
            result.scaleFactor = 1.0;
        } else {
            // 7. Collect a sequence of characters that are not "," (U+002C) characters, and let that be descriptors.
            skipWhile<CharType, isHTMLSpace<CharType> >(position, attributeEnd);
            const CharType* descriptorsStart = position;
            skipUntil<CharType, isComma<CharType> >(position, attributeEnd);
            const CharType* descriptorsEnd = position;
            if (!parseDescriptors(descriptorsStart, descriptorsEnd, result))
                continue;
        }

        ASSERT(imageURLEnd > attributeStart);
        unsigned imageURLStartingPosition = imageURLStart - attributeStart;
        ASSERT(imageURLEnd > imageURLStart);
        unsigned imageURLLength = imageURLEnd - imageURLStart;
        imageCandidates.append(ImageCandidate(attribute, imageURLStartingPosition, imageURLLength, result, ImageCandidate::SrcsetOrigin));
        // 11. Return to the step labeled splitting loop.
    }
}

static void parseImageCandidatesFromSrcsetAttribute(const String& attribute, Vector<ImageCandidate>& imageCandidates)
{
    if (attribute.isNull())
        return;

    if (attribute.is8Bit())
        parseImageCandidatesFromSrcsetAttribute<LChar>(attribute, attribute.characters8(), attribute.length(), imageCandidates);
    else
        parseImageCandidatesFromSrcsetAttribute<UChar>(attribute, attribute.characters16(), attribute.length(), imageCandidates);
}

static ImageCandidate pickBestImageCandidate(float deviceScaleFactor, int effectiveSize, Vector<ImageCandidate>& imageCandidates)
{
    bool ignoreSrc = false;
    if (imageCandidates.isEmpty())
        return ImageCandidate();

    // http://picture.responsiveimages.org/#normalize-source-densities
    for (Vector<ImageCandidate>::iterator it = imageCandidates.begin(); it != imageCandidates.end(); ++it) {
        if (it->resourceWidth() > 0) {
            it->setScaleFactor((float)it->resourceWidth() / (float)effectiveSize);
            ignoreSrc = true;
        }
    }

    std::stable_sort(imageCandidates.begin(), imageCandidates.end(), compareByScaleFactor);

    unsigned i;
    for (i = 0; i < imageCandidates.size() - 1; ++i) {
        if ((imageCandidates[i].scaleFactor() >= deviceScaleFactor) && (!ignoreSrc || !imageCandidates[i].srcOrigin()))
            break;
    }

    float winningScaleFactor = imageCandidates[i].scaleFactor();
    unsigned winner = i;
    // 16. If an entry b in candidates has the same associated ... pixel density as an earlier entry a in candidates,
    // then remove entry b
    while ((i > 0) && (imageCandidates[--i].scaleFactor() == winningScaleFactor))
        winner = i;

    return imageCandidates[winner];
}

ImageCandidate bestFitSourceForSrcsetAttribute(float deviceScaleFactor, int effectiveSize, const String& srcsetAttribute)
{
    Vector<ImageCandidate> imageCandidates;

    parseImageCandidatesFromSrcsetAttribute(srcsetAttribute, imageCandidates);

    return pickBestImageCandidate(deviceScaleFactor, effectiveSize, imageCandidates);
}

ImageCandidate bestFitSourceForImageAttributes(float deviceScaleFactor, int effectiveSize, const String& srcAttribute, const String& srcsetAttribute)
{
    DescriptorParsingResult defaultResult;
    defaultResult.scaleFactor = 1.0;

    if (srcsetAttribute.isNull()) {
        if (srcAttribute.isNull())
            return ImageCandidate();
        return ImageCandidate(srcAttribute, 0, srcAttribute.length(), defaultResult, ImageCandidate::SrcOrigin);
    }

    Vector<ImageCandidate> imageCandidates;

    parseImageCandidatesFromSrcsetAttribute(srcsetAttribute, imageCandidates);

    if (!srcAttribute.isEmpty())
        imageCandidates.append(ImageCandidate(srcAttribute, 0, srcAttribute.length(), defaultResult, ImageCandidate::SrcOrigin));

    return pickBestImageCandidate(deviceScaleFactor, effectiveSize, imageCandidates);
}

String bestFitSourceForImageAttributes(float deviceScaleFactor, int effectiveSize, const String& srcAttribute, ImageCandidate& srcsetImageCandidate)
{
    DescriptorParsingResult defaultResult;
    defaultResult.scaleFactor = 1.0;

    if (srcsetImageCandidate.isEmpty())
        return srcAttribute;

    Vector<ImageCandidate> imageCandidates;
    imageCandidates.append(srcsetImageCandidate);

    if (!srcAttribute.isEmpty())
        imageCandidates.append(ImageCandidate(srcAttribute, 0, srcAttribute.length(), defaultResult, ImageCandidate::SrcOrigin));

    return pickBestImageCandidate(deviceScaleFactor, effectiveSize, imageCandidates).toString();
}

}
