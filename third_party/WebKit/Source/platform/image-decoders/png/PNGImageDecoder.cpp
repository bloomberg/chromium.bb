/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Portions are Copyright (C) 2001 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "platform/image-decoders/png/PNGImageDecoder.h"

namespace blink {

PNGImageDecoder::PNGImageDecoder(AlphaOption alphaOption,
                                 const ColorBehavior& colorBehavior,
                                 size_t maxDecodedBytes,
                                 size_t offset)
    : ImageDecoder(alphaOption, colorBehavior, maxDecodedBytes),
      m_offset(offset),
      m_currentFrame(0),
      // It would be logical to default to cAnimationNone, but BitmapImage uses
      // that as a signal to never check again, meaning the actual count will
      // never be respected.
      m_repetitionCount(cAnimationLoopOnce),
      m_hasAlphaChannel(false),
      m_currentBufferSawAlpha(false) {}

PNGImageDecoder::~PNGImageDecoder() {}

bool PNGImageDecoder::setFailed() {
  m_reader.reset();
  return ImageDecoder::setFailed();
}

size_t PNGImageDecoder::decodeFrameCount() {
  parse(ParseQuery::MetaData);
  return failed() ? m_frameBufferCache.size() : m_reader->frameCount();
}

void PNGImageDecoder::decode(size_t index) {
  parse(ParseQuery::MetaData);

  if (failed())
    return;

  updateAggressivePurging(index);

  Vector<size_t> framesToDecode = findFramesToDecode(index);
  for (auto i = framesToDecode.rbegin(); i != framesToDecode.rend(); i++) {
    m_currentFrame = *i;
    if (!m_reader->decode(*m_data, *i)) {
      setFailed();
      return;
    }

    // If this returns false, we need more data to continue decoding.
    if (!postDecodeProcessing(*i))
      break;
  }

  // It is also a fatal error if all data is received and we have decoded all
  // frames available but the file is truncated.
  if (index >= m_frameBufferCache.size() - 1 && isAllDataReceived() &&
      m_reader && !m_reader->parseCompleted())
    setFailed();
}

void PNGImageDecoder::parse(ParseQuery query) {
  if (failed() || (m_reader && m_reader->parseCompleted()))
    return;

  if (!m_reader)
    m_reader = WTF::makeUnique<PNGImageReader>(this, m_offset);

  if (!m_reader->parse(*m_data, query))
    setFailed();
}

void PNGImageDecoder::clearFrameBuffer(size_t index) {
  if (m_reader)
    m_reader->clearDecodeState(index);
  ImageDecoder::clearFrameBuffer(index);
}

bool PNGImageDecoder::canReusePreviousFrameBuffer(size_t index) const {
  DCHECK(index < m_frameBufferCache.size());
  return m_frameBufferCache[index].getDisposalMethod() !=
         ImageFrame::DisposeOverwritePrevious;
}

void PNGImageDecoder::setRepetitionCount(int repetitionCount) {
  m_repetitionCount = repetitionCount;
}

int PNGImageDecoder::repetitionCount() const {
  return failed() ? cAnimationLoopOnce : m_repetitionCount;
}

void PNGImageDecoder::initializeNewFrame(size_t index) {
  const PNGImageReader::FrameInfo& frameInfo = m_reader->frameInfo(index);
  ImageFrame& buffer = m_frameBufferCache[index];

  DCHECK(IntRect(IntPoint(), size()).contains(frameInfo.frameRect));
  buffer.setOriginalFrameRect(frameInfo.frameRect);

  buffer.setDuration(frameInfo.duration);
  buffer.setDisposalMethod(frameInfo.disposalMethod);
  buffer.setAlphaBlendSource(frameInfo.alphaBlend);

  size_t previousFrameIndex = findRequiredPreviousFrame(index, false);
  buffer.setRequiredPreviousFrameIndex(previousFrameIndex);
}

inline sk_sp<SkColorSpace> readColorSpace(png_structp png, png_infop info) {
  if (png_get_valid(png, info, PNG_INFO_sRGB))
    return SkColorSpace::MakeSRGB();

  png_charp name;
  int compression;
  png_bytep profile;
  png_uint_32 length;
  if (png_get_iCCP(png, info, &name, &compression, &profile, &length))
    return SkColorSpace::MakeICC(profile, length);

  png_fixed_point chrm[8];
  if (!png_get_cHRM_fixed(png, info, &chrm[0], &chrm[1], &chrm[2], &chrm[3],
                          &chrm[4], &chrm[5], &chrm[6], &chrm[7]))
    return nullptr;

  png_fixed_point inverseGamma;
  if (!png_get_gAMA_fixed(png, info, &inverseGamma))
    return nullptr;

  // cHRM and gAMA tags are both present. The PNG spec states that cHRM is
  // valid even without gAMA but we cannot apply the cHRM without guessing
  // a gAMA. Color correction is not a guessing game: match the behavior
  // of Safari and Firefox instead (compat).

  struct pngFixedToFloat {
    explicit pngFixedToFloat(png_fixed_point value)
        : floatValue(.00001f * value) {}
    operator float() { return floatValue; }
    float floatValue;
  };

  SkColorSpacePrimaries primaries;
  primaries.fRX = pngFixedToFloat(chrm[2]);
  primaries.fRY = pngFixedToFloat(chrm[3]);
  primaries.fGX = pngFixedToFloat(chrm[4]);
  primaries.fGY = pngFixedToFloat(chrm[5]);
  primaries.fBX = pngFixedToFloat(chrm[6]);
  primaries.fBY = pngFixedToFloat(chrm[7]);
  primaries.fWX = pngFixedToFloat(chrm[0]);
  primaries.fWY = pngFixedToFloat(chrm[1]);

  SkMatrix44 toXYZD50(SkMatrix44::kUninitialized_Constructor);
  if (!primaries.toXYZD50(&toXYZD50))
    return nullptr;

  SkColorSpaceTransferFn fn;
  fn.fG = 1.0f / pngFixedToFloat(inverseGamma);
  fn.fA = 1.0f;
  fn.fB = fn.fC = fn.fD = fn.fE = fn.fF = 0.0f;

  return SkColorSpace::MakeRGB(fn, toXYZD50);
}

void PNGImageDecoder::headerAvailable() {
  png_structp png = m_reader->pngPtr();
  png_infop info = m_reader->infoPtr();

  png_uint_32 width, height;
  int bitDepth, colorType, interlaceType, compressionType;
  png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType,
               &interlaceType, &compressionType, nullptr);

  // The options we set here match what Mozilla does.

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (colorType == PNG_COLOR_TYPE_PALETTE ||
      (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8))
    png_set_expand(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_expand(png);

  if (bitDepth == 16)
    png_set_strip_16(png);

  if (colorType == PNG_COLOR_TYPE_GRAY ||
      colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  // Only set the size and the color space of the image once since non-first
  // frames also use this method: there is no per-frame color space, and the
  // image size is determined from the header width and height.
  if (!isDecodedSizeAvailable()) {
    // Protect against large PNGs. See http://bugzil.la/251381 for more details.
    const unsigned long maxPNGSize = 1000000UL;
    if (width > maxPNGSize || height > maxPNGSize) {
      longjmp(JMPBUF(png), 1);
      return;
    }

    // Set the image size now that the image header is available.
    if (!setSize(width, height)) {
      longjmp(JMPBUF(png), 1);
      return;
    }

    if ((colorType & PNG_COLOR_MASK_COLOR) && !ignoresColorSpace()) {
      // We only support color profiles for color PALETTE and RGB[A] PNG.
      // TODO(msarret): Add GRAY profile support, block CYMK?
      if (sk_sp<SkColorSpace> colorSpace = readColorSpace(png, info))
        setEmbeddedColorSpace(colorSpace);
    }
  }

  if (!hasEmbeddedColorSpace()) {
    const double inverseGamma = 0.45455;
    const double defaultGamma = 2.2;
    double gamma;
    if (!ignoresColorSpace() && png_get_gAMA(png, info, &gamma)) {
      const double maxGamma = 21474.83;
      if ((gamma <= 0.0) || (gamma > maxGamma)) {
        gamma = inverseGamma;
        png_set_gAMA(png, info, gamma);
      }
      png_set_gamma(png, defaultGamma, gamma);
    } else {
      png_set_gamma(png, defaultGamma, inverseGamma);
    }
  }

  DCHECK(isDecodedSizeAvailable());

  // Tell libpng to send us rows for interlaced pngs.
  if (interlaceType == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png);

  // Update our info now (so we can get color channel info).
  png_read_update_info(png, info);

  int channels = png_get_channels(png, info);
  DCHECK(channels == 3 || channels == 4);
  m_hasAlphaChannel = (channels == 4);
}

void PNGImageDecoder::rowAvailable(unsigned char* rowBuffer,
                                   unsigned rowIndex,
                                   int) {
  if (m_currentFrame >= m_frameBufferCache.size())
    return;

  ImageFrame& buffer = m_frameBufferCache[m_currentFrame];
  if (buffer.getStatus() == ImageFrame::FrameEmpty) {
    png_structp png = m_reader->pngPtr();
    if (!initFrameBuffer(m_currentFrame)) {
      longjmp(JMPBUF(png), 1);
      return;
    }

    DCHECK_EQ(ImageFrame::FramePartial, buffer.getStatus());

    if (PNG_INTERLACE_ADAM7 ==
        png_get_interlace_type(png, m_reader->infoPtr())) {
      unsigned colorChannels = m_hasAlphaChannel ? 4 : 3;
      m_reader->createInterlaceBuffer(colorChannels * size().area());
      if (!m_reader->interlaceBuffer()) {
        longjmp(JMPBUF(png), 1);
        return;
      }
    }

    m_currentBufferSawAlpha = false;
  }

  const IntRect& frameRect = buffer.originalFrameRect();
  DCHECK(IntRect(IntPoint(), size()).contains(frameRect));

  /* libpng comments (here to explain what follows).
   *
   * this function is called for every row in the image. If the
   * image is interlacing, and you turned on the interlace handler,
   * this function will be called for every row in every pass.
   * Some of these rows will not be changed from the previous pass.
   * When the row is not changed, the new_row variable will be NULL.
   * The rows and passes are called in order, so you don't really
   * need the row_num and pass, but I'm supplying them because it
   * may make your life easier.
   */

  // Nothing to do if the row is unchanged, or the row is outside the image
  // bounds. In the case that a frame presents more data than the indicated
  // frame size, ignore the extra rows and use the frame size as the source
  // of truth. libpng can send extra rows: ignore them too, this to prevent
  // memory writes outside of the image bounds (security).
  if (!rowBuffer)
    return;

  DCHECK_GT(frameRect.height(), 0);
  if (rowIndex >= static_cast<unsigned>(frameRect.height()))
    return;

  int y = rowIndex + frameRect.y();
  if (y < 0)
    return;
  DCHECK_LT(y, size().height());

  /* libpng comments (continued).
   *
   * For the non-NULL rows of interlaced images, you must call
   * png_progressive_combine_row() passing in the row and the
   * old row.  You can call this function for NULL rows (it will
   * just return) and for non-interlaced images (it just does the
   * memcpy for you) if it will make the code easier. Thus, you
   * can just do this for all cases:
   *
   *    png_progressive_combine_row(png_ptr, old_row, new_row);
   *
   * where old_row is what was displayed for previous rows. Note
   * that the first pass (pass == 0 really) will completely cover
   * the old row, so the rows do not have to be initialized. After
   * the first pass (and only for interlaced images), you will have
   * to pass the current row, and the function will combine the
   * old row and the new row.
   */

  bool hasAlpha = m_hasAlphaChannel;
  png_bytep row = rowBuffer;

  if (png_bytep interlaceBuffer = m_reader->interlaceBuffer()) {
    unsigned colorChannels = hasAlpha ? 4 : 3;
    row = interlaceBuffer + (rowIndex * colorChannels * size().width());
    png_progressive_combine_row(m_reader->pngPtr(), row, rowBuffer);
  }

  // Write the decoded row pixels to the frame buffer. The repetitive
  // form of the row write loops is for speed.
  ImageFrame::PixelData* const dstRow = buffer.getAddr(frameRect.x(), y);
  const int width = frameRect.width();

  png_bytep srcPtr = row;
  if (hasAlpha) {
    // Here we apply the color space transformation to the dst space.
    // It does not really make sense to transform to a gamma-encoded
    // space and then immediately after, perform a linear premultiply.
    // Ideally we would pass kPremul_SkAlphaType to xform->apply(),
    // instructing SkColorSpaceXform to perform the linear premultiply
    // while the pixels are a linear space.
    // We cannot do this because when we apply the gamma encoding after
    // the premultiply, we will very likely end up with valid pixels
    // where R, G, and/or B are greater than A.  The legacy drawing
    // pipeline does not know how to handle this.
    if (SkColorSpaceXform* xform = colorTransform()) {
      SkColorSpaceXform::ColorFormat colorFormat =
          SkColorSpaceXform::kRGBA_8888_ColorFormat;
      xform->apply(colorFormat, dstRow, colorFormat, srcPtr, width,
                   kUnpremul_SkAlphaType);
      srcPtr = png_bytep(dstRow);
    }

    unsigned alphaMask = 255;
    if (m_frameBufferCache[m_currentFrame].getAlphaBlendSource() ==
        ImageFrame::BlendAtopBgcolor) {
      if (buffer.premultiplyAlpha()) {
        for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
             dstPixel++, srcPtr += 4) {
          buffer.setRGBAPremultiply(dstPixel, srcPtr[0], srcPtr[1], srcPtr[2],
                                    srcPtr[3]);
          alphaMask &= srcPtr[3];
        }
      } else {
        for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
             dstPixel++, srcPtr += 4) {
          buffer.setRGBARaw(dstPixel, srcPtr[0], srcPtr[1], srcPtr[2],
                            srcPtr[3]);
          alphaMask &= srcPtr[3];
        }
      }
    } else {
      // Now, the blend method is ImageFrame::BlendAtopPreviousFrame. Since the
      // frame data of the previous frame is copied at initFrameBuffer, we can
      // blend the pixel of this frame, stored in |srcPtr|, over the previous
      // pixel stored in |dstPixel|.
      if (buffer.premultiplyAlpha()) {
        for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
             dstPixel++, srcPtr += 4) {
          buffer.blendRGBAPremultiplied(dstPixel, srcPtr[0], srcPtr[1],
                                        srcPtr[2], srcPtr[3]);
          alphaMask &= srcPtr[3];
        }
      } else {
        for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
             dstPixel++, srcPtr += 4) {
          buffer.blendRGBARaw(dstPixel, srcPtr[0], srcPtr[1], srcPtr[2],
                              srcPtr[3]);
          alphaMask &= srcPtr[3];
        }
      }
    }

    if (alphaMask != 255)
      m_currentBufferSawAlpha = true;

  } else {
    for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
         srcPtr += 3, ++dstPixel) {
      buffer.setRGBARaw(dstPixel, srcPtr[0], srcPtr[1], srcPtr[2], 255);
    }

    // We'll apply the color space xform to opaque pixels after they have been
    // written to the ImageFrame, purely because SkColorSpaceXform supports
    // RGBA (and not RGB).
    if (SkColorSpaceXform* xform = colorTransform()) {
      xform->apply(xformColorFormat(), dstRow, xformColorFormat(), dstRow,
                   width, kOpaque_SkAlphaType);
    }
  }

  buffer.setPixelsChanged(true);
}

void PNGImageDecoder::frameComplete() {
  if (m_currentFrame >= m_frameBufferCache.size())
    return;

  if (m_reader->interlaceBuffer())
    m_reader->clearInterlaceBuffer();

  ImageFrame& buffer = m_frameBufferCache[m_currentFrame];
  if (buffer.getStatus() == ImageFrame::FrameEmpty) {
    longjmp(JMPBUF(m_reader->pngPtr()), 1);
    return;
  }

  if (!m_currentBufferSawAlpha)
    correctAlphaWhenFrameBufferSawNoAlpha(m_currentFrame);

  buffer.setStatus(ImageFrame::FrameComplete);
}

bool PNGImageDecoder::frameIsCompleteAtIndex(size_t index) const {
  if (!isDecodedSizeAvailable())
    return false;

  DCHECK(!failed() && m_reader);

  // For non-animated images, return whether the status of the frame is
  // ImageFrame::FrameComplete with ImageDecoder::frameIsCompleteAtIndex.
  // This matches the behavior of WEBPImageDecoder.
  if (m_reader->parseCompleted() && m_reader->frameCount() == 1)
    return ImageDecoder::frameIsCompleteAtIndex(index);

  return m_reader->frameIsReceivedAtIndex(index);
}

float PNGImageDecoder::frameDurationAtIndex(size_t index) const {
  if (index < m_frameBufferCache.size())
    return m_frameBufferCache[index].duration();
  return 0;
}

}  // namespace blink
