/*
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/platform/image-decoders/ImageDecoder.h"

#include "core/platform/PlatformMemoryInstrumentation.h"
#include "core/platform/SharedBuffer.h"
#include "core/platform/image-decoders/bmp/BMPImageDecoder.h"
#include "core/platform/image-decoders/gif/GIFImageDecoder.h"
#include "core/platform/image-decoders/ico/ICOImageDecoder.h"
#include "core/platform/image-decoders/jpeg/JPEGImageDecoder.h"
#include "core/platform/image-decoders/png/PNGImageDecoder.h"
#include "core/platform/image-decoders/webp/WEBPImageDecoder.h"

#include <algorithm>
#include <cmath>
#include <wtf/MemoryInstrumentationVector.h>

using namespace std;

namespace WebCore {

static unsigned copyFromSharedBuffer(char* buffer, unsigned bufferLength, const SharedBuffer& sharedBuffer, unsigned offset)
{
    unsigned bytesExtracted = 0;
    const char* moreData;
    while (unsigned moreDataLength = sharedBuffer.getSomeData(moreData, offset)) {
        unsigned bytesToCopy = min(bufferLength - bytesExtracted, moreDataLength);
        memcpy(buffer + bytesExtracted, moreData, bytesToCopy);
        bytesExtracted += bytesToCopy;
        if (bytesExtracted == bufferLength)
            break;
        offset += bytesToCopy;
    }
    return bytesExtracted;
}

inline bool matchesGIFSignature(char* contents)
{
    return !memcmp(contents, "GIF87a", 6) || !memcmp(contents, "GIF89a", 6);
}

inline bool matchesPNGSignature(char* contents)
{
    return !memcmp(contents, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8);
}

inline bool matchesJPEGSignature(char* contents)
{
    return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

inline bool matchesWebPSignature(char* contents)
{
    return !memcmp(contents, "RIFF", 4) && !memcmp(contents + 8, "WEBPVP", 6);
}

inline bool matchesBMPSignature(char* contents)
{
    return !memcmp(contents, "BM", 2);
}

inline bool matchesICOSignature(char* contents)
{
    return !memcmp(contents, "\x00\x00\x01\x00", 4);
}

inline bool matchesCURSignature(char* contents)
{
    return !memcmp(contents, "\x00\x00\x02\x00", 4);
}

PassOwnPtr<ImageDecoder> ImageDecoder::create(const SharedBuffer& data, ImageSource::AlphaOption alphaOption, ImageSource::GammaAndColorProfileOption gammaAndColorProfileOption)
{
    static const unsigned lengthOfLongestSignature = 14; // To wit: "RIFF????WEBPVP"
    char contents[lengthOfLongestSignature];
    unsigned length = copyFromSharedBuffer(contents, lengthOfLongestSignature, data, 0);
    if (length < lengthOfLongestSignature)
        return nullptr;

    if (matchesGIFSignature(contents))
        return adoptPtr(new GIFImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesPNGSignature(contents))
        return adoptPtr(new PNGImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesICOSignature(contents) || matchesCURSignature(contents))
        return adoptPtr(new ICOImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesJPEGSignature(contents))
        return adoptPtr(new JPEGImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesWebPSignature(contents))
        return adoptPtr(new WEBPImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesBMPSignature(contents))
        return adoptPtr(new BMPImageDecoder(alphaOption, gammaAndColorProfileOption));

    return nullptr;
}

bool ImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    return !frameIsCompleteAtIndex(index) || m_frameBufferCache[index].hasAlpha();
}

bool ImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    return (index < m_frameBufferCache.size()) &&
        (m_frameBufferCache[index].status() == ImageFrame::FrameComplete);
}

unsigned ImageDecoder::frameBytesAtIndex(size_t index) const
{
    if (m_frameBufferCache.size() <= index)
        return 0;
    // FIXME: Use the dimension of the requested frame.
    return m_size.area() * sizeof(ImageFrame::PixelData);
}

void ImageDecoder::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, PlatformMemoryTypes::Image);
    info.addMember(m_data, "data");
    info.addMember(m_frameBufferCache, "frameBufferCache");
    info.addMember(m_colorProfile, "colorProfile");
    info.addMember(m_scaledColumns, "scaledColumns");
    info.addMember(m_scaledRows, "scaledRows");
}

enum MatchType {
    Exact,
    UpperBound,
    LowerBound
};

inline void fillScaledValues(Vector<int>& scaledValues, double scaleRate, int length)
{
    double inflateRate = 1. / scaleRate;
    scaledValues.reserveCapacity(static_cast<int>(length * scaleRate + 0.5));
    for (int scaledIndex = 0; ; ++scaledIndex) {
        int index = static_cast<int>(scaledIndex * inflateRate + 0.5);
        if (index >= length)
            break;
        scaledValues.append(index);
    }
}

template <MatchType type> static int getScaledValue(const Vector<int>& scaledValues, int valueToMatch, int searchStart)
{
    if (scaledValues.isEmpty())
        return valueToMatch;

    const int* dataStart = scaledValues.data();
    const int* dataEnd = dataStart + scaledValues.size();
    const int* matched = std::lower_bound(dataStart + searchStart, dataEnd, valueToMatch);
    switch (type) {
    case Exact:
        return matched != dataEnd && *matched == valueToMatch ? matched - dataStart : -1;
    case LowerBound:
        return matched != dataEnd && *matched == valueToMatch ? matched - dataStart : matched - dataStart - 1;
    case UpperBound:
    default:
        return matched != dataEnd ? matched - dataStart : -1;
    }
}

void ImageDecoder::prepareScaleDataIfNecessary()
{
    m_scaled = false;
    m_scaledColumns.clear();
    m_scaledRows.clear();

    int width = size().width();
    int height = size().height();
    int numPixels = height * width;
    if (m_maxNumPixels <= 0 || numPixels <= m_maxNumPixels)
        return;

    m_scaled = true;
    double scale = sqrt(m_maxNumPixels / (double)numPixels);
    fillScaledValues(m_scaledColumns, scale, width);
    fillScaledValues(m_scaledRows, scale, height);
}

int ImageDecoder::upperBoundScaledX(int origX, int searchStart)
{
    return getScaledValue<UpperBound>(m_scaledColumns, origX, searchStart);
}

int ImageDecoder::lowerBoundScaledX(int origX, int searchStart)
{
    return getScaledValue<LowerBound>(m_scaledColumns, origX, searchStart);
}

int ImageDecoder::upperBoundScaledY(int origY, int searchStart)
{
    return getScaledValue<UpperBound>(m_scaledRows, origY, searchStart);
}

int ImageDecoder::lowerBoundScaledY(int origY, int searchStart)
{
    return getScaledValue<LowerBound>(m_scaledRows, origY, searchStart);
}

int ImageDecoder::scaledY(int origY, int searchStart)
{
    return getScaledValue<Exact>(m_scaledRows, origY, searchStart);
}

} // namespace WebCore
