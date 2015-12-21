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

#include "platform/graphics/ImageFrameGenerator.h"

#include "SkData.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/image-decoders/ImageDecoder.h"

namespace blink {

static bool compatibleInfo(const SkImageInfo& src, const SkImageInfo& dst)
{
    if (src == dst)
        return true;

    // It is legal to write kOpaque_SkAlphaType pixels into a kPremul_SkAlphaType buffer.
    // This can happen when DeferredImageDecoder allocates an kOpaque_SkAlphaType image
    // generator based on cached frame info, while the ImageFrame-allocated dest bitmap
    // stays kPremul_SkAlphaType.
    if (src.alphaType() == kOpaque_SkAlphaType && dst.alphaType() == kPremul_SkAlphaType) {
        const SkImageInfo& tmp = src.makeAlphaType(kPremul_SkAlphaType);
        return tmp == dst;
    }

    return false;
}

// Creates a SkPixelRef such that the memory for pixels is given by an external body.
// This is used to write directly to the memory given by Skia during decoding.
class ImageFrameGenerator::ExternalMemoryAllocator final : public SkBitmap::Allocator {
    USING_FAST_MALLOC(ExternalMemoryAllocator);
    WTF_MAKE_NONCOPYABLE(ExternalMemoryAllocator);
public:
    ExternalMemoryAllocator(const SkImageInfo& info, void* pixels, size_t rowBytes)
        : m_info(info)
        , m_pixels(pixels)
        , m_rowBytes(rowBytes)
    {
    }

    bool allocPixelRef(SkBitmap* dst, SkColorTable* ctable) override
    {
        const SkImageInfo& info = dst->info();
        if (kUnknown_SkColorType == info.colorType())
            return false;

        if (!compatibleInfo(m_info, info) || m_rowBytes != dst->rowBytes())
            return false;

        if (!dst->installPixels(info, m_pixels, m_rowBytes))
            return false;
        dst->lockPixels();
        return true;
    }

private:
    SkImageInfo m_info;
    void* m_pixels;
    size_t m_rowBytes;
};

static bool updateYUVComponentSizes(ImageDecoder* decoder, SkISize componentSizes[3], ImageDecoder::SizeType sizeType)
{
    if (!decoder->canDecodeToYUV())
        return false;

    IntSize size = decoder->decodedYUVSize(0, sizeType);
    componentSizes[0].set(size.width(), size.height());
    size = decoder->decodedYUVSize(1, sizeType);
    componentSizes[1].set(size.width(), size.height());
    size = decoder->decodedYUVSize(2, sizeType);
    componentSizes[2].set(size.width(), size.height());
    return true;
}

ImageFrameGenerator::ImageFrameGenerator(const SkISize& fullSize, PassRefPtr<SharedBuffer> data, bool allDataReceived, bool isMultiFrame)
    : m_fullSize(fullSize)
    , m_data(adoptRef(new ThreadSafeDataTransport()))
    , m_isMultiFrame(isMultiFrame)
    , m_decodeFailed(false)
    , m_frameCount(0)
    , m_encodedData(nullptr)
{
    setData(data.get(), allDataReceived);
}

ImageFrameGenerator::~ImageFrameGenerator()
{
    if (m_encodedData)
        m_encodedData->unref();
    ImageDecodingStore::instance().removeCacheIndexedByGenerator(this);
}

void ImageFrameGenerator::setData(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    m_data->setData(data.get(), allDataReceived);
}

static void sharedSkDataReleaseCallback(const void* address, void* context)
{
    // This gets called when m_encodedData reference count becomes 0 - and it could happen in
    // ImageFrameGenerator destructor or later when m_encodedData gets dereferenced.
    // In this method, we deref ThreadSafeDataTransport, as ThreadSafeDataTransport is the owner
    // of data returned via refEncodedData.

    ThreadSafeDataTransport* dataTransport = static_cast<ThreadSafeDataTransport*>(context);
#if ENABLE(ASSERT)
    ASSERT(dataTransport);
    SharedBuffer* buffer = 0;
    bool allDataReceived = false;
    dataTransport->data(&buffer, &allDataReceived);
    ASSERT(allDataReceived && buffer && buffer->data() == address);
#endif
    // Dereference m_data now.
    dataTransport->deref();
}

SkData* ImageFrameGenerator::refEncodedData()
{
    // SkData is returned only when full image (encoded) data is received. This is important
    // since DeferredImageDecoder::setData is called only once with allDataReceived set to true,
    // and after that m_data->m_readBuffer.data() is not changed. See also RELEASE_ASSERT used in
    // ThreadSafeDataTransport::data().
    SharedBuffer* buffer = 0;
    bool allDataReceived = false;
    m_data->data(&buffer, &allDataReceived);
    if (!allDataReceived)
        return nullptr;

    {
        // Prevents concurrent access to m_encodedData creation.
        MutexLocker lock(m_decodeMutex);
        if (m_encodedData) {
            m_encodedData->ref();
            return m_encodedData;
        }
        // m_encodedData is created with initial reference count == 1. ImageFrameGenerator always holds one
        // reference to m_encodedData, as it prevents write access in SkData::writable_data.
        m_encodedData = SkData::NewWithProc(buffer->data(), buffer->size(), sharedSkDataReleaseCallback, m_data.get());
        // While m_encodedData is referenced, prevent disposing m_data and its content.
        // it is dereferenced in sharedSkDataReleaseCallback, called when m_encodedData gets dereferenced.
        m_data->ref();
    }
    // Increase the reference, caller must decrease it. One reference is always kept by ImageFrameGenerator and released
    // in destructor.
    m_encodedData->ref();
    return m_encodedData;
}

bool ImageFrameGenerator::decodeAndScale(size_t index, const SkImageInfo& info, void* pixels, size_t rowBytes)
{
    // Prevent concurrent decode or scale operations on the same image data.
    MutexLocker lock(m_decodeMutex);

    if (m_decodeFailed)
        return false;

    TRACE_EVENT1("blink", "ImageFrameGenerator::decodeAndScale", "frame index", static_cast<int>(index));

    m_externalAllocator = adoptPtr(new ExternalMemoryAllocator(info, pixels, rowBytes));

    // This implementation does not support scaling so check the requested size.
    SkISize scaledSize = SkISize::Make(info.width(), info.height());
    ASSERT(m_fullSize == scaledSize);

    SkBitmap bitmap = tryToResumeDecode(index, scaledSize);
    if (bitmap.isNull())
        return false;

    // Don't keep the allocator because it contains a pointer to memory
    // that we do not own.
    m_externalAllocator.clear();

    // Check to see if the decoder has written directly to the pixel memory
    // provided. If not, make a copy.
    ASSERT(bitmap.width() == scaledSize.width());
    ASSERT(bitmap.height() == scaledSize.height());
    SkAutoLockPixels bitmapLock(bitmap);
    if (bitmap.getPixels() != pixels)
        return bitmap.copyPixelsTo(pixels, rowBytes * info.height(), rowBytes);
    return true;
}

bool ImageFrameGenerator::decodeToYUV(size_t index, SkISize componentSizes[3], void* planes[3], size_t rowBytes[3])
{
    // Prevent concurrent decode or scale operations on the same image data.
    MutexLocker lock(m_decodeMutex);

    if (m_decodeFailed)
        return false;

    TRACE_EVENT1("blink", "ImageFrameGenerator::decodeToYUV", "frame index", static_cast<int>(index));

    if (!planes || !planes[0] || !planes[1] || !planes[2]
        || !rowBytes || !rowBytes[0] || !rowBytes[1] || !rowBytes[2]) {
        return false;
    }

    SharedBuffer* data = 0;
    bool allDataReceived = false;
    m_data->data(&data, &allDataReceived);

    // FIXME: YUV decoding does not currently support progressive decoding.
    ASSERT(allDataReceived);

    OwnPtr<ImageDecoder> decoder = ImageDecoder::create(*data, ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied);
    if (!decoder)
        return false;

    decoder->setData(data, allDataReceived);

    OwnPtr<ImagePlanes> imagePlanes = adoptPtr(new ImagePlanes(planes, rowBytes));
    decoder->setImagePlanes(imagePlanes.release());

    bool sizeUpdated = updateYUVComponentSizes(decoder.get(), componentSizes, ImageDecoder::ActualSize);
    RELEASE_ASSERT(sizeUpdated);

    if (decoder->decodeToYUV()) {
        setHasAlpha(0, false); // YUV is always opaque
        return true;
    }

    ASSERT(decoder->failed());
    m_decodeFailed = true;
    return false;
}

SkBitmap ImageFrameGenerator::tryToResumeDecode(size_t index, const SkISize& scaledSize)
{
    TRACE_EVENT1("blink", "ImageFrameGenerator::tryToResumeDecode", "frame index", static_cast<int>(index));

    ImageDecoder* decoder = 0;
    const bool resumeDecoding = ImageDecodingStore::instance().lockDecoder(this, m_fullSize, &decoder);
    ASSERT(!resumeDecoding || decoder);

    SkBitmap fullSizeImage;
    bool complete = decode(index, &decoder, &fullSizeImage);

    if (!decoder)
        return SkBitmap();
    if (index >= m_frameComplete.size())
        m_frameComplete.resize(index + 1);
    m_frameComplete[index] = complete;

    // If we are not resuming decoding that means the decoder is freshly
    // created and we have ownership. If we are resuming decoding then
    // the decoder is owned by ImageDecodingStore.
    OwnPtr<ImageDecoder> decoderContainer;
    if (!resumeDecoding)
        decoderContainer = adoptPtr(decoder);

    if (fullSizeImage.isNull()) {
        // If decoding has failed, we can save work in the future by
        // ignoring further requests to decode the image.
        m_decodeFailed = decoder->failed();
        if (resumeDecoding)
            ImageDecodingStore::instance().unlockDecoder(this, decoder);
        return SkBitmap();
    }

    // If the image generated is complete then there is no need to keep
    // the decoder. For multi-frame images, if all frames in the image are
    // decoded, we remove the decoder.
    bool removeDecoder;

    if (m_isMultiFrame) {
        size_t decodedFrameCount = 0;
        for (Vector<bool>::iterator it = m_frameComplete.begin(); it != m_frameComplete.end(); ++it) {
            if (*it)
                decodedFrameCount++;
        }
        removeDecoder = m_frameCount && (decodedFrameCount == m_frameCount);
    } else {
        removeDecoder = complete;
    }

    if (resumeDecoding) {
        if (removeDecoder) {
            ImageDecodingStore::instance().removeDecoder(this, decoder);
            m_frameComplete.clear();
        } else {
            ImageDecodingStore::instance().unlockDecoder(this, decoder);
        }
    } else if (!removeDecoder) {
        ImageDecodingStore::instance().insertDecoder(this, decoderContainer.release());
    }
    return fullSizeImage;
}

void ImageFrameGenerator::setHasAlpha(size_t index, bool hasAlpha)
{
    MutexLocker lock(m_alphaMutex);
    if (index >= m_hasAlpha.size()) {
        const size_t oldSize = m_hasAlpha.size();
        m_hasAlpha.resize(index + 1);
        for (size_t i = oldSize; i < m_hasAlpha.size(); ++i)
            m_hasAlpha[i] = true;
    }
    m_hasAlpha[index] = hasAlpha;
}

bool ImageFrameGenerator::decode(size_t index, ImageDecoder** decoder, SkBitmap* bitmap)
{
    TRACE_EVENT2("blink", "ImageFrameGenerator::decode", "width", m_fullSize.width(), "height", m_fullSize.height());

    SharedBuffer* data = 0;
    bool allDataReceived = false;
    m_data->data(&data, &allDataReceived);

    // Try to create an ImageDecoder if we are not given one.
    ASSERT(decoder);
    bool newDecoder = false;
    if (!*decoder) {
        newDecoder = true;
        if (m_imageDecoderFactory)
            *decoder = m_imageDecoderFactory->create().leakPtr();

        if (!*decoder)
            *decoder = ImageDecoder::create(*data, ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied).leakPtr();

        if (!*decoder)
            return false;
    }

    if (!m_isMultiFrame && newDecoder && allDataReceived) {
        // If we're using an external memory allocator that means we're decoding
        // directly into the output memory and we can save one memcpy.
        ASSERT(m_externalAllocator.get());
        (*decoder)->setMemoryAllocator(m_externalAllocator.get());
    }

    (*decoder)->setData(data, allDataReceived);
    ImageFrame* frame = (*decoder)->frameBufferAtIndex(index);

    // For multi-frame image decoders, we need to know how many frames are
    // in that image in order to release the decoder when all frames are
    // decoded. frameCount() is reliable only if all data is received and set in
    // decoder, particularly with GIF.
    if (allDataReceived)
        m_frameCount = (*decoder)->frameCount();

    (*decoder)->setData(0, false); // Unref SharedBuffer from ImageDecoder.
    (*decoder)->clearCacheExceptFrame(index);
    (*decoder)->setMemoryAllocator(0);

    if (!frame || frame->status() == ImageFrame::FrameEmpty)
        return false;

    // A cache object is considered complete if we can decode a complete frame.
    // Or we have received all data. The image might not be fully decoded in
    // the latter case.
    const bool isDecodeComplete = frame->status() == ImageFrame::FrameComplete || allDataReceived;

    SkBitmap fullSizeBitmap = frame->getSkBitmap();
    if (!fullSizeBitmap.isNull()) {
        ASSERT(fullSizeBitmap.width() == m_fullSize.width() && fullSizeBitmap.height() == m_fullSize.height());
        setHasAlpha(index, !fullSizeBitmap.isOpaque());
    }

    *bitmap = fullSizeBitmap;
    return isDecodeComplete;
}

bool ImageFrameGenerator::hasAlpha(size_t index)
{
    MutexLocker lock(m_alphaMutex);
    if (index < m_hasAlpha.size())
        return m_hasAlpha[index];
    return true;
}

bool ImageFrameGenerator::getYUVComponentSizes(SkISize componentSizes[3])
{
    TRACE_EVENT2("blink", "ImageFrameGenerator::getYUVComponentSizes", "width", m_fullSize.width(), "height", m_fullSize.height());

    SharedBuffer* data = 0;
    bool allDataReceived = false;
    m_data->data(&data, &allDataReceived);

    // FIXME: YUV decoding does not currently support progressive decoding.
    if (!allDataReceived)
        return false;

    OwnPtr<ImageDecoder> decoder = ImageDecoder::create(*data, ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied);
    if (!decoder)
        return false;

    // Setting a dummy ImagePlanes object signals to the decoder that we want to do YUV decoding.
    decoder->setData(data, allDataReceived);
    OwnPtr<ImagePlanes> dummyImagePlanes = adoptPtr(new ImagePlanes);
    decoder->setImagePlanes(dummyImagePlanes.release());

    ASSERT(componentSizes);
    return updateYUVComponentSizes(decoder.get(), componentSizes, ImageDecoder::SizeForMemoryAllocation);
}

} // namespace blink
