/*
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

#include "platform/image-decoders/ImageDecoder.h"

#include "platform/PlatformInstrumentation.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "platform/image-decoders/FastSharedBufferReader.h"
#include "platform/image-decoders/bmp/BMPImageDecoder.h"
#include "platform/image-decoders/gif/GIFImageDecoder.h"
#include "platform/image-decoders/ico/ICOImageDecoder.h"
#include "platform/image-decoders/jpeg/JPEGImageDecoder.h"
#include "platform/image-decoders/png/PNGImageDecoder.h"
#include "platform/image-decoders/webp/WEBPImageDecoder.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

inline bool matchesJPEGSignature(const char* contents) {
  return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

inline bool matchesPNGSignature(const char* contents) {
  return !memcmp(contents, "\x89PNG\r\n\x1A\n", 8);
}

inline bool matchesGIFSignature(const char* contents) {
  return !memcmp(contents, "GIF87a", 6) || !memcmp(contents, "GIF89a", 6);
}

inline bool matchesWebPSignature(const char* contents) {
  return !memcmp(contents, "RIFF", 4) && !memcmp(contents + 8, "WEBPVP", 6);
}

inline bool matchesICOSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x01\x00", 4);
}

inline bool matchesCURSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x02\x00", 4);
}

inline bool matchesBMPSignature(const char* contents) {
  return !memcmp(contents, "BM", 2);
}

// This needs to be updated if we ever add a matches*Signature() which requires
// more characters.
static constexpr size_t kLongestSignatureLength = sizeof("RIFF????WEBPVP") - 1;

std::unique_ptr<ImageDecoder> ImageDecoder::create(
    PassRefPtr<SegmentReader> passData,
    bool dataComplete,
    AlphaOption alphaOption,
    const ColorBehavior& colorBehavior) {
  RefPtr<SegmentReader> data = passData;

  // We need at least kLongestSignatureLength bytes to run the signature
  // matcher.
  if (data->size() < kLongestSignatureLength)
    return nullptr;

  const size_t maxDecodedBytes =
      Platform::current() ? Platform::current()->maxDecodedImageBytes()
                          : noDecodedImageByteLimit;

  // Access the first kLongestSignatureLength chars to sniff the signature.
  // (note: FastSharedBufferReader only makes a copy if the bytes are segmented)
  char buffer[kLongestSignatureLength];
  const FastSharedBufferReader fastReader(data);
  const ImageDecoder::SniffResult sniffResult = determineImageType(
      fastReader.getConsecutiveData(0, kLongestSignatureLength, buffer),
      kLongestSignatureLength);

  std::unique_ptr<ImageDecoder> decoder;
  switch (sniffResult) {
    case SniffResult::JPEG:
      decoder.reset(
          new JPEGImageDecoder(alphaOption, colorBehavior, maxDecodedBytes));
      break;
    case SniffResult::PNG:
      decoder.reset(
          new PNGImageDecoder(alphaOption, colorBehavior, maxDecodedBytes));
      break;
    case SniffResult::GIF:
      decoder.reset(
          new GIFImageDecoder(alphaOption, colorBehavior, maxDecodedBytes));
      break;
    case SniffResult::WEBP:
      decoder.reset(
          new WEBPImageDecoder(alphaOption, colorBehavior, maxDecodedBytes));
      break;
    case SniffResult::ICO:
      decoder.reset(
          new ICOImageDecoder(alphaOption, colorBehavior, maxDecodedBytes));
      break;
    case SniffResult::BMP:
      decoder.reset(
          new BMPImageDecoder(alphaOption, colorBehavior, maxDecodedBytes));
      break;
    case SniffResult::Invalid:
      break;
  }

  if (decoder)
    decoder->setData(data.release(), dataComplete);

  return decoder;
}

bool ImageDecoder::hasSufficientDataToSniffImageType(const SharedBuffer& data) {
  return data.size() >= kLongestSignatureLength;
}

ImageDecoder::SniffResult ImageDecoder::determineImageType(const char* contents,
                                                           size_t length) {
  DCHECK_GE(length, kLongestSignatureLength);

  if (matchesJPEGSignature(contents))
    return SniffResult::JPEG;
  if (matchesPNGSignature(contents))
    return SniffResult::PNG;
  if (matchesGIFSignature(contents))
    return SniffResult::GIF;
  if (matchesWebPSignature(contents))
    return SniffResult::WEBP;
  if (matchesICOSignature(contents) || matchesCURSignature(contents))
    return SniffResult::ICO;
  if (matchesBMPSignature(contents))
    return SniffResult::BMP;
  return SniffResult::Invalid;
}

size_t ImageDecoder::frameCount() {
  const size_t oldSize = m_frameBufferCache.size();
  const size_t newSize = decodeFrameCount();
  if (oldSize != newSize) {
    m_frameBufferCache.resize(newSize);
    for (size_t i = oldSize; i < newSize; ++i) {
      m_frameBufferCache[i].setPremultiplyAlpha(m_premultiplyAlpha);
      initializeNewFrame(i);
    }
  }
  return newSize;
}

ImageFrame* ImageDecoder::frameBufferAtIndex(size_t index) {
  if (index >= frameCount())
    return 0;

  ImageFrame* frame = &m_frameBufferCache[index];
  if (frame->getStatus() != ImageFrame::FrameComplete) {
    PlatformInstrumentation::willDecodeImage(filenameExtension());
    decode(index);
    PlatformInstrumentation::didDecodeImage();
  }

  if (!m_hasHistogrammedColorSpace) {
    BitmapImageMetrics::countImageGammaAndGamut(m_embeddedColorSpace.get());
    m_hasHistogrammedColorSpace = true;
  }

  frame->notifyBitmapIfPixelsChanged();
  return frame;
}

bool ImageDecoder::frameHasAlphaAtIndex(size_t index) const {
  return !frameIsCompleteAtIndex(index) || m_frameBufferCache[index].hasAlpha();
}

bool ImageDecoder::frameIsCompleteAtIndex(size_t index) const {
  return (index < m_frameBufferCache.size()) &&
         (m_frameBufferCache[index].getStatus() == ImageFrame::FrameComplete);
}

size_t ImageDecoder::frameBytesAtIndex(size_t index) const {
  if (index >= m_frameBufferCache.size() ||
      m_frameBufferCache[index].getStatus() == ImageFrame::FrameEmpty)
    return 0;

  struct ImageSize {
    explicit ImageSize(IntSize size) {
      area = static_cast<uint64_t>(size.width()) * size.height();
    }

    uint64_t area;
  };

  return ImageSize(frameSizeAtIndex(index)).area *
         sizeof(ImageFrame::PixelData);
}

size_t ImageDecoder::clearCacheExceptFrame(size_t clearExceptFrame) {
  // Don't clear if there are no frames or only one frame.
  if (m_frameBufferCache.size() <= 1)
    return 0;

  // We expect that after this call, we'll be asked to decode frames after this
  // one. So we want to avoid clearing frames such that those requests would
  // force re-decoding from the beginning of the image. There are two cases in
  // which preserving |clearCacheExcept| frame is not enough to avoid that:
  //
  // 1. |clearExceptFrame| is not yet sufficiently decoded to decode subsequent
  //    frames. We need the previous frame to sufficiently decode this frame.
  // 2. The disposal method of |clearExceptFrame| is DisposeOverwritePrevious.
  //    In that case, we need to keep the required previous frame in the cache
  //    to prevent re-decoding that frame when |clearExceptFrame| is disposed.
  //
  // If either 1 or 2 is true, store the required previous frame in
  // |clearExceptFrame2| so it won't be cleared.
  size_t clearExceptFrame2 = kNotFound;
  if (clearExceptFrame < m_frameBufferCache.size()) {
    const ImageFrame& frame = m_frameBufferCache[clearExceptFrame];
    if (!frameStatusSufficientForSuccessors(clearExceptFrame) ||
        frame.getDisposalMethod() == ImageFrame::DisposeOverwritePrevious)
      clearExceptFrame2 = frame.requiredPreviousFrameIndex();
  }

  // Now |clearExceptFrame2| indicates the frame that |clearExceptFrame|
  // depends on, as described above. But if decoding is skipping forward past
  // intermediate frames, this frame may be insufficiently decoded. So we need
  // to keep traversing back through the required previous frames until we find
  // the nearest ancestor that is sufficiently decoded. Preserving that will
  // minimize the amount of future decoding needed.
  while (clearExceptFrame2 < m_frameBufferCache.size() &&
         !frameStatusSufficientForSuccessors(clearExceptFrame2)) {
    clearExceptFrame2 =
        m_frameBufferCache[clearExceptFrame2].requiredPreviousFrameIndex();
  }

  return clearCacheExceptTwoFrames(clearExceptFrame, clearExceptFrame2);
}

size_t ImageDecoder::clearCacheExceptTwoFrames(size_t clearExceptFrame1,
                                               size_t clearExceptFrame2) {
  size_t frameBytesCleared = 0;
  for (size_t i = 0; i < m_frameBufferCache.size(); ++i) {
    if (m_frameBufferCache[i].getStatus() != ImageFrame::FrameEmpty &&
        i != clearExceptFrame1 && i != clearExceptFrame2) {
      frameBytesCleared += frameBytesAtIndex(i);
      clearFrameBuffer(i);
    }
  }
  return frameBytesCleared;
}

void ImageDecoder::clearFrameBuffer(size_t frameIndex) {
  m_frameBufferCache[frameIndex].clearPixelData();
}

Vector<size_t> ImageDecoder::findFramesToDecode(size_t index) const {
  DCHECK(index < m_frameBufferCache.size());

  Vector<size_t> framesToDecode;
  do {
    framesToDecode.push_back(index);
    index = m_frameBufferCache[index].requiredPreviousFrameIndex();
  } while (index != kNotFound &&
           m_frameBufferCache[index].getStatus() != ImageFrame::FrameComplete);
  return framesToDecode;
}

bool ImageDecoder::postDecodeProcessing(size_t index) {
  DCHECK(index < m_frameBufferCache.size());

  if (m_frameBufferCache[index].getStatus() != ImageFrame::FrameComplete)
    return false;

  if (m_purgeAggressively)
    clearCacheExceptFrame(index);

  return true;
}

void ImageDecoder::correctAlphaWhenFrameBufferSawNoAlpha(size_t index) {
  DCHECK(index < m_frameBufferCache.size());
  ImageFrame& buffer = m_frameBufferCache[index];

  // When this frame spans the entire image rect we can set hasAlpha to false,
  // since there are logically no transparent pixels outside of the frame rect.
  if (buffer.originalFrameRect().contains(IntRect(IntPoint(), size()))) {
    buffer.setHasAlpha(false);
    buffer.setRequiredPreviousFrameIndex(kNotFound);
  } else if (buffer.requiredPreviousFrameIndex() != kNotFound) {
    // When the frame rect does not span the entire image rect, and it does
    // *not* have a required previous frame, the pixels outside of the frame
    // rect will be fully transparent, so we shoudn't set hasAlpha to false.
    //
    // It is a tricky case when the frame does have a required previous frame.
    // The frame does not have alpha only if everywhere outside its rect
    // doesn't have alpha.  To know whether this is true, we check the start
    // state of the frame -- if it doesn't have alpha, we're safe.
    //
    // We first check that the required previous frame does not have
    // DisposeOverWritePrevious as its disposal method - this should never
    // happen, since the required frame should in that case be the required
    // frame of this frame's required frame.
    //
    // If |prevBuffer| is DisposeNotSpecified or DisposeKeep, |buffer| has no
    // alpha if |prevBuffer| had no alpha. Since initFrameBuffer() already
    // copied the alpha state, there's nothing to do here.
    //
    // The only remaining case is a DisposeOverwriteBgcolor frame.  If
    // it had no alpha, and its rect is contained in the current frame's
    // rect, we know the current frame has no alpha.
    //
    // For DisposeNotSpecified, DisposeKeep and DisposeOverwriteBgcolor there
    // is one situation that is not taken into account - when |prevBuffer|
    // *does* have alpha, but only in the frame rect of |buffer|, we can still
    // say that this frame has no alpha. However, to determine this, we
    // potentially need to analyze all image pixels of |prevBuffer|, which is
    // too computationally expensive.
    const ImageFrame* prevBuffer =
        &m_frameBufferCache[buffer.requiredPreviousFrameIndex()];
    DCHECK(prevBuffer->getDisposalMethod() !=
           ImageFrame::DisposeOverwritePrevious);

    if ((prevBuffer->getDisposalMethod() ==
         ImageFrame::DisposeOverwriteBgcolor) &&
        !prevBuffer->hasAlpha() &&
        buffer.originalFrameRect().contains(prevBuffer->originalFrameRect()))
      buffer.setHasAlpha(false);
  }
}

bool ImageDecoder::initFrameBuffer(size_t frameIndex) {
  DCHECK(frameIndex < m_frameBufferCache.size());

  ImageFrame* const buffer = &m_frameBufferCache[frameIndex];

  // If the frame is already initialized, return true.
  if (buffer->getStatus() != ImageFrame::FrameEmpty)
    return true;

  size_t requiredPreviousFrameIndex = buffer->requiredPreviousFrameIndex();
  if (requiredPreviousFrameIndex == kNotFound) {
    // This frame doesn't rely on any previous data.
    if (!buffer->setSizeAndColorSpace(size().width(), size().height(),
                                      colorSpaceForSkImages())) {
      return setFailed();
    }
  } else {
    ImageFrame* const prevBuffer =
        &m_frameBufferCache[requiredPreviousFrameIndex];
    DCHECK(prevBuffer->getStatus() == ImageFrame::FrameComplete);

    // We try to reuse |prevBuffer| as starting state to avoid copying.
    // If canReusePreviousFrameBuffer returns false, we must copy the data since
    // |prevBuffer| is necessary to decode this or later frames. In that case,
    // copy the data instead.
    if ((!canReusePreviousFrameBuffer(frameIndex) ||
         !buffer->takeBitmapDataIfWritable(prevBuffer)) &&
        !buffer->copyBitmapData(*prevBuffer))
      return setFailed();

    if (prevBuffer->getDisposalMethod() ==
        ImageFrame::DisposeOverwriteBgcolor) {
      // We want to clear the previous frame to transparent, without
      // affecting pixels in the image outside of the frame.
      const IntRect& prevRect = prevBuffer->originalFrameRect();
      DCHECK(!prevRect.contains(IntRect(IntPoint(), size())));
      buffer->zeroFillFrameRect(prevRect);
    }
  }

  // Update our status to be partially complete.
  buffer->setStatus(ImageFrame::FramePartial);

  onInitFrameBuffer(frameIndex);
  return true;
}

void ImageDecoder::updateAggressivePurging(size_t index) {
  if (m_purgeAggressively)
    return;

  // We don't want to cache so much that we cause a memory issue.
  //
  // If we used a LRU cache we would fill it and then on next animation loop
  // we would need to decode all the frames again -- the LRU would give no
  // benefit and would consume more memory.
  // So instead, simply purge unused frames if caching all of the frames of
  // the image would use more memory than the image decoder is allowed
  // (m_maxDecodedBytes) or would overflow 32 bits..
  //
  // As we decode we will learn the total number of frames, and thus total
  // possible image memory used.

  const uint64_t frameArea = decodedSize().area();
  const uint64_t frameMemoryUsage = frameArea * 4;  // 4 bytes per pixel
  if (frameMemoryUsage / 4 != frameArea) {          // overflow occurred
    m_purgeAggressively = true;
    return;
  }

  const uint64_t totalMemoryUsage = frameMemoryUsage * index;
  if (totalMemoryUsage / frameMemoryUsage != index) {  // overflow occurred
    m_purgeAggressively = true;
    return;
  }

  if (totalMemoryUsage > m_maxDecodedBytes) {
    m_purgeAggressively = true;
  }
}

size_t ImageDecoder::findRequiredPreviousFrame(size_t frameIndex,
                                               bool frameRectIsOpaque) {
  DCHECK_LT(frameIndex, m_frameBufferCache.size());
  if (!frameIndex) {
    // The first frame doesn't rely on any previous data.
    return kNotFound;
  }

  const ImageFrame* currBuffer = &m_frameBufferCache[frameIndex];
  if ((frameRectIsOpaque ||
       currBuffer->getAlphaBlendSource() == ImageFrame::BlendAtopBgcolor) &&
      currBuffer->originalFrameRect().contains(IntRect(IntPoint(), size())))
    return kNotFound;

  // The starting state for this frame depends on the previous frame's
  // disposal method.
  size_t prevFrame = frameIndex - 1;
  const ImageFrame* prevBuffer = &m_frameBufferCache[prevFrame];

  switch (prevBuffer->getDisposalMethod()) {
    case ImageFrame::DisposeNotSpecified:
    case ImageFrame::DisposeKeep:
      // prevFrame will be used as the starting state for this frame.
      // FIXME: Be even smarter by checking the frame sizes and/or
      // alpha-containing regions.
      return prevFrame;
    case ImageFrame::DisposeOverwritePrevious:
      // Frames that use the DisposeOverwritePrevious method are effectively
      // no-ops in terms of changing the starting state of a frame compared to
      // the starting state of the previous frame, so skip over them and
      // return the required previous frame of it.
      return prevBuffer->requiredPreviousFrameIndex();
    case ImageFrame::DisposeOverwriteBgcolor:
      // If the previous frame fills the whole image, then the current frame
      // can be decoded alone. Likewise, if the previous frame could be
      // decoded without reference to any prior frame, the starting state for
      // this frame is a blank frame, so it can again be decoded alone.
      // Otherwise, the previous frame contributes to this frame.
      return (prevBuffer->originalFrameRect().contains(
                  IntRect(IntPoint(), size())) ||
              (prevBuffer->requiredPreviousFrameIndex() == kNotFound))
                 ? kNotFound
                 : prevFrame;
    default:
      NOTREACHED();
      return kNotFound;
  }
}

ImagePlanes::ImagePlanes() {
  for (int i = 0; i < 3; ++i) {
    m_planes[i] = 0;
    m_rowBytes[i] = 0;
  }
}

ImagePlanes::ImagePlanes(void* planes[3], const size_t rowBytes[3]) {
  for (int i = 0; i < 3; ++i) {
    m_planes[i] = planes[i];
    m_rowBytes[i] = rowBytes[i];
  }
}

void* ImagePlanes::plane(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, 3);
  return m_planes[i];
}

size_t ImagePlanes::rowBytes(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, 3);
  return m_rowBytes[i];
}

void ImageDecoder::setEmbeddedColorProfile(const char* iccData,
                                           unsigned iccLength) {
  sk_sp<SkColorSpace> colorSpace = SkColorSpace::MakeICC(iccData, iccLength);
  if (!colorSpace)
    DLOG(ERROR) << "Failed to parse image ICC profile";
  setEmbeddedColorSpace(std::move(colorSpace));
}

void ImageDecoder::setEmbeddedColorSpace(sk_sp<SkColorSpace> colorSpace) {
  DCHECK(!ignoresColorSpace());
  DCHECK(!m_hasHistogrammedColorSpace);

  m_embeddedColorSpace = colorSpace;
  m_sourceToTargetColorTransformNeedsUpdate = true;
}

SkColorSpaceXform* ImageDecoder::colorTransform() {
  if (!m_sourceToTargetColorTransformNeedsUpdate)
    return m_sourceToTargetColorTransform.get();
  m_sourceToTargetColorTransformNeedsUpdate = false;
  m_sourceToTargetColorTransform = nullptr;

  if (!m_colorBehavior.isTransformToTargetColorSpace())
    return nullptr;

  sk_sp<SkColorSpace> srcColorSpace = m_embeddedColorSpace;
  if (!srcColorSpace) {
    if (RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
      srcColorSpace = SkColorSpace::MakeSRGB();
    else
      return nullptr;
  }

  sk_sp<SkColorSpace> dstColorSpace =
      m_colorBehavior.targetColorSpace().ToSkColorSpace();

  if (SkColorSpace::Equals(srcColorSpace.get(), dstColorSpace.get())) {
    return nullptr;
  }

  m_sourceToTargetColorTransform =
      SkColorSpaceXform::New(srcColorSpace.get(), dstColorSpace.get());
  return m_sourceToTargetColorTransform.get();
}

sk_sp<SkColorSpace> ImageDecoder::colorSpaceForSkImages() const {
  if (!m_colorBehavior.isTag())
    return nullptr;

  if (m_embeddedColorSpace)
    return m_embeddedColorSpace;
  return SkColorSpace::MakeSRGB();
}

}  // namespace blink
