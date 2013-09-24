/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/platform/graphics/chromium/ImageFrameGenerator.h"

#include "core/platform/SharedBuffer.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/graphics/chromium/DiscardablePixelRef.h"
#include "core/platform/graphics/chromium/ImageDecodingStore.h"
#include "core/platform/graphics/chromium/ScaledImageFragment.h"
#include "core/platform/image-decoders/ImageDecoder.h"

#include "skia/ext/image_operations.h"

namespace WebCore {

namespace {

skia::ImageOperations::ResizeMethod resizeMethod()
{
    return skia::ImageOperations::RESIZE_LANCZOS3;
}

} // namespace

ImageFrameGenerator::ImageFrameGenerator(const SkISize& fullSize, PassRefPtr<SharedBuffer> data, bool allDataReceived, bool isMultiFrame)
    : m_fullSize(fullSize)
    , m_isMultiFrame(isMultiFrame)
    , m_decodeFailedAndEmpty(false)
    , m_decodeCount(ScaledImageFragment::FirstPartialImage)
{
    setData(data.get(), allDataReceived);
}

ImageFrameGenerator::~ImageFrameGenerator()
{
    // FIXME: This check is not really thread-safe. This should be changed to:
    // ImageDecodingStore::removeCacheFromInstance(this);
    // Which uses a lock internally.
    if (ImageDecodingStore::instance())
        ImageDecodingStore::instance()->removeCacheIndexedByGenerator(this);
}

void ImageFrameGenerator::setData(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    m_data.setData(data.get(), allDataReceived);
}

void ImageFrameGenerator::copyData(RefPtr<SharedBuffer>* data, bool* allDataReceived)
{
    SharedBuffer* buffer = 0;
    m_data.data(&buffer, allDataReceived);
    if (buffer)
        *data = buffer->copy();
}

const ScaledImageFragment* ImageFrameGenerator::decodeAndScale(const SkISize& scaledSize, size_t index)
{
    // Prevents concurrent decode or scale operations on the same image data.
    // Multiple LazyDecodingPixelRefs can call this method at the same time.
    MutexLocker lock(m_decodeMutex);
    if (m_decodeFailedAndEmpty)
        return 0;

    const ScaledImageFragment* cachedImage = 0;

    cachedImage = tryToLockCompleteCache(scaledSize, index);
    if (cachedImage)
        return cachedImage;

    TRACE_EVENT2("webkit", "ImageFrameGenerator::decodeAndScale", "generator", this, "decodeCount", static_cast<int>(m_decodeCount));

    cachedImage = tryToScale(0, scaledSize, index);
    if (cachedImage)
        return cachedImage;

    cachedImage = tryToResumeDecodeAndScale(scaledSize, index);
    if (cachedImage)
        return cachedImage;
    return 0;
}

const ScaledImageFragment* ImageFrameGenerator::tryToLockCompleteCache(const SkISize& scaledSize, size_t index)
{
    const ScaledImageFragment* cachedImage = 0;
    if (ImageDecodingStore::instance()->lockCache(this, scaledSize, index, &cachedImage))
        return cachedImage;
    return 0;
}

const ScaledImageFragment* ImageFrameGenerator::tryToScale(const ScaledImageFragment* fullSizeImage, const SkISize& scaledSize, size_t index)
{
    TRACE_EVENT0("webkit", "ImageFrameGenerator::tryToScale");

    // If the requested scaled size is the same as the full size then exit
    // early. This saves a cache lookup.
    if (scaledSize == m_fullSize)
        return 0;

    if (!fullSizeImage && !ImageDecodingStore::instance()->lockCache(this, m_fullSize, index, &fullSizeImage))
        return 0;

    // This call allocates the DiscardablePixelRef and lock/unlocks it
    // afterwards. So the memory allocated to the scaledBitmap can be
    // discarded after this call. Need to lock the scaledBitmap and
    // check the pixels before using it next time.
    SkBitmap scaledBitmap = skia::ImageOperations::Resize(fullSizeImage->bitmap(), resizeMethod(), scaledSize.width(), scaledSize.height(), &m_allocator);

    OwnPtr<ScaledImageFragment> scaledImage;
    if (fullSizeImage->isComplete())
        scaledImage = ScaledImageFragment::createComplete(scaledSize, fullSizeImage->index(), scaledBitmap);
    else
        scaledImage = ScaledImageFragment::createPartial(scaledSize, fullSizeImage->index(), nextGenerationId(), scaledBitmap);
    ImageDecodingStore::instance()->unlockCache(this, fullSizeImage);
    return ImageDecodingStore::instance()->insertAndLockCache(this, scaledImage.release());
}

const ScaledImageFragment* ImageFrameGenerator::tryToResumeDecodeAndScale(const SkISize& scaledSize, size_t index)
{
    TRACE_EVENT1("webkit", "ImageFrameGenerator::tryToResumeDecodeAndScale", "index", static_cast<int>(index));

    ImageDecoder* decoder = 0;
    const bool resumeDecoding = ImageDecodingStore::instance()->lockDecoder(this, m_fullSize, &decoder);
    ASSERT(!resumeDecoding || decoder);

    OwnPtr<ScaledImageFragment> fullSizeImage = decode(index, &decoder);

    if (!decoder)
        return 0;

    // If we are not resuming decoding that means the decoder is freshly
    // created and we have ownership. If we are resuming decoding then
    // the decoder is owned by ImageDecodingStore.
    OwnPtr<ImageDecoder> decoderContainer;
    if (!resumeDecoding)
        decoderContainer = adoptPtr(decoder);

    if (!fullSizeImage) {
        // If decode has failed and resulted an empty image we can save work
        // in the future by returning early.
        m_decodeFailedAndEmpty = !m_isMultiFrame && decoder->failed();

        if (resumeDecoding)
            ImageDecodingStore::instance()->unlockDecoder(this, decoder);
        return 0;
    }

    const ScaledImageFragment* cachedImage = ImageDecodingStore::instance()->insertAndLockCache(this, fullSizeImage.release());

    // If the image generated is complete then there is no need to keep
    // the decoder. The exception is multi-frame decoder which can generate
    // multiple complete frames.
    const bool removeDecoder = cachedImage->isComplete() && !m_isMultiFrame;

    if (resumeDecoding) {
        if (removeDecoder)
            ImageDecodingStore::instance()->removeDecoder(this, decoder);
        else
            ImageDecodingStore::instance()->unlockDecoder(this, decoder);
    } else if (!removeDecoder) {
        ImageDecodingStore::instance()->insertDecoder(this, decoderContainer.release(), DiscardablePixelRef::isDiscardable(cachedImage->bitmap().pixelRef()));
    }

    if (m_fullSize == scaledSize)
        return cachedImage;
    return tryToScale(cachedImage, scaledSize, index);
}

PassOwnPtr<ScaledImageFragment> ImageFrameGenerator::decode(size_t index, ImageDecoder** decoder)
{
    TRACE_EVENT2("webkit", "ImageFrameGenerator::decode", "width", m_fullSize.width(), "height", m_fullSize.height());

    ASSERT(decoder);
    SharedBuffer* data = 0;
    bool allDataReceived = false;
    m_data.data(&data, &allDataReceived);

    // Try to create an ImageDecoder if we are not given one.
    if (!*decoder) {
        if (m_imageDecoderFactory)
            *decoder = m_imageDecoderFactory->create().leakPtr();

        if (!*decoder)
            *decoder = ImageDecoder::create(*data, ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileApplied).leakPtr();

        if (!*decoder)
            return nullptr;
    }

    // TODO: this is very ugly. We need to refactor the way how we can pass a
    // memory allocator to image decoders.
    if (!m_isMultiFrame)
        (*decoder)->setMemoryAllocator(&m_allocator);
    (*decoder)->setData(data, allDataReceived);
    // If this call returns a newly allocated DiscardablePixelRef, then
    // ImageFrame::m_bitmap and the contained DiscardablePixelRef are locked.
    // They will be unlocked when ImageDecoder is destroyed since ImageDecoder
    // owns the ImageFrame. Partially decoded SkBitmap is thus inserted into the
    // ImageDecodingStore while locked.
    ImageFrame* frame = (*decoder)->frameBufferAtIndex(index);
    (*decoder)->setData(0, false); // Unref SharedBuffer from ImageDecoder.
    (*decoder)->clearCacheExceptFrame(index);

    if (!frame || frame->status() == ImageFrame::FrameEmpty)
        return nullptr;

    const bool isComplete = frame->status() == ImageFrame::FrameComplete;
    SkBitmap fullSizeBitmap = frame->getSkBitmap();
    {
        MutexLocker lock(m_alphaMutex);
        if (index >= m_hasAlpha.size()) {
            const size_t oldSize = m_hasAlpha.size();
            m_hasAlpha.resize(index + 1);
            for (size_t i = oldSize; i < m_hasAlpha.size(); ++i)
                m_hasAlpha[i] = true;
        }
        m_hasAlpha[index] = !fullSizeBitmap.isOpaque();
    }
    ASSERT(fullSizeBitmap.width() == m_fullSize.width() && fullSizeBitmap.height() == m_fullSize.height());

    if (isComplete)
        return ScaledImageFragment::createComplete(m_fullSize, index, fullSizeBitmap);

    // If the image is partial we need to return a copy. This is to avoid future
    // decode operations writing to the same bitmap.
    SkBitmap copyBitmap;
    fullSizeBitmap.copyTo(&copyBitmap, fullSizeBitmap.config(), &m_allocator);
    return ScaledImageFragment::createPartial(m_fullSize, index, nextGenerationId(), copyBitmap);
}

bool ImageFrameGenerator::hasAlpha(size_t index)
{
    MutexLocker lock(m_alphaMutex);
    if (index < m_hasAlpha.size())
        return m_hasAlpha[index];
    return true;
}

} // namespace WebCore
