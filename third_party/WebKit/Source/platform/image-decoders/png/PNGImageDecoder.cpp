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

PNGImageDecoder::PNGImageDecoder(AlphaOption alpha_option,
                                 const ColorBehavior& color_behavior,
                                 size_t max_decoded_bytes,
                                 size_t offset)
    : ImageDecoder(alpha_option, color_behavior, max_decoded_bytes),
      offset_(offset),
      current_frame_(0),
      // It would be logical to default to kAnimationNone, but BitmapImage uses
      // that as a signal to never check again, meaning the actual count will
      // never be respected.
      repetition_count_(kAnimationLoopOnce),
      has_alpha_channel_(false),
      current_buffer_saw_alpha_(false) {}

PNGImageDecoder::~PNGImageDecoder() {}

bool PNGImageDecoder::SetFailed() {
  reader_.reset();
  return ImageDecoder::SetFailed();
}

size_t PNGImageDecoder::DecodeFrameCount() {
  Parse(ParseQuery::kMetaData);
  return Failed() ? frame_buffer_cache_.size() : reader_->FrameCount();
}

void PNGImageDecoder::Decode(size_t index) {
  Parse(ParseQuery::kMetaData);

  if (Failed())
    return;

  UpdateAggressivePurging(index);

  Vector<size_t> frames_to_decode = FindFramesToDecode(index);
  for (auto i = frames_to_decode.rbegin(); i != frames_to_decode.rend(); i++) {
    current_frame_ = *i;
    if (!reader_->Decode(*data_, *i)) {
      SetFailed();
      return;
    }

    // If this returns false, we need more data to continue decoding.
    if (!PostDecodeProcessing(*i))
      break;
  }

  // It is also a fatal error if all data is received and we have decoded all
  // frames available but the file is truncated.
  if (index >= frame_buffer_cache_.size() - 1 && IsAllDataReceived() &&
      reader_ && !reader_->ParseCompleted())
    SetFailed();
}

void PNGImageDecoder::Parse(ParseQuery query) {
  if (Failed() || (reader_ && reader_->ParseCompleted()))
    return;

  if (!reader_)
    reader_ = WTF::MakeUnique<PNGImageReader>(this, offset_);

  if (!reader_->Parse(*data_, query))
    SetFailed();
}

void PNGImageDecoder::ClearFrameBuffer(size_t index) {
  if (reader_)
    reader_->ClearDecodeState(index);
  ImageDecoder::ClearFrameBuffer(index);
}

bool PNGImageDecoder::CanReusePreviousFrameBuffer(size_t index) const {
  DCHECK(index < frame_buffer_cache_.size());
  return frame_buffer_cache_[index].GetDisposalMethod() !=
         ImageFrame::kDisposeOverwritePrevious;
}

void PNGImageDecoder::SetRepetitionCount(int repetition_count) {
  repetition_count_ = repetition_count;
}

int PNGImageDecoder::RepetitionCount() const {
  return Failed() ? kAnimationLoopOnce : repetition_count_;
}

void PNGImageDecoder::InitializeNewFrame(size_t index) {
  const PNGImageReader::FrameInfo& frame_info = reader_->GetFrameInfo(index);
  ImageFrame& buffer = frame_buffer_cache_[index];

  DCHECK(IntRect(IntPoint(), Size()).Contains(frame_info.frame_rect));
  buffer.SetOriginalFrameRect(frame_info.frame_rect);

  buffer.SetDuration(frame_info.duration);
  buffer.SetDisposalMethod(frame_info.disposal_method);
  buffer.SetAlphaBlendSource(frame_info.alpha_blend);

  size_t previous_frame_index = FindRequiredPreviousFrame(index, false);
  buffer.SetRequiredPreviousFrameIndex(previous_frame_index);
}

inline sk_sp<SkColorSpace> ReadColorSpace(png_structp png, png_infop info) {
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

  png_fixed_point inverse_gamma;
  if (!png_get_gAMA_fixed(png, info, &inverse_gamma))
    return nullptr;

  // cHRM and gAMA tags are both present. The PNG spec states that cHRM is
  // valid even without gAMA but we cannot apply the cHRM without guessing
  // a gAMA. Color correction is not a guessing game: match the behavior
  // of Safari and Firefox instead (compat).

  struct pngFixedToFloat {
    explicit pngFixedToFloat(png_fixed_point value)
        : float_value(.00001f * value) {}
    operator float() { return float_value; }
    float float_value;
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

  SkMatrix44 to_xyzd50(SkMatrix44::kUninitialized_Constructor);
  if (!primaries.toXYZD50(&to_xyzd50))
    return nullptr;

  SkColorSpaceTransferFn fn;
  fn.fG = 1.0f / pngFixedToFloat(inverse_gamma);
  fn.fA = 1.0f;
  fn.fB = fn.fC = fn.fD = fn.fE = fn.fF = 0.0f;

  return SkColorSpace::MakeRGB(fn, to_xyzd50);
}

void PNGImageDecoder::SetColorSpace() {
  if (IgnoresColorSpace())
    return;
  png_structp png = reader_->PngPtr();
  png_infop info = reader_->InfoPtr();
  const int color_type = png_get_color_type(png, info);
  if (!(color_type & PNG_COLOR_MASK_COLOR))
    return;
  // We only support color profiles for color PALETTE and RGB[A] PNG.
  // TODO(msarett): Add GRAY profile support, block CYMK?
  sk_sp<SkColorSpace> color_space = ReadColorSpace(png, info);
  if (color_space)
    SetEmbeddedColorSpace(color_space);
}

bool PNGImageDecoder::SetSize(unsigned width, unsigned height) {
  DCHECK(!IsDecodedSizeAvailable());
  // Protect against large PNGs. See http://bugzil.la/251381 for more details.
  const unsigned long kMaxPNGSize = 1000000UL;
  return (width <= kMaxPNGSize) && (height <= kMaxPNGSize) &&
         ImageDecoder::SetSize(width, height);
}

void PNGImageDecoder::HeaderAvailable() {
  DCHECK(IsDecodedSizeAvailable());

  png_structp png = reader_->PngPtr();
  png_infop info = reader_->InfoPtr();

  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type, compression_type;
  png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
               &interlace_type, &compression_type, nullptr);

  // The options we set here match what Mozilla does.

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    png_set_expand(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_expand(png);

  if (bit_depth == 16)
    png_set_strip_16(png);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  if (!HasEmbeddedColorSpace()) {
    const double kInverseGamma = 0.45455;
    const double kDefaultGamma = 2.2;
    double gamma;
    if (!IgnoresColorSpace() && png_get_gAMA(png, info, &gamma)) {
      const double kMaxGamma = 21474.83;
      if ((gamma <= 0.0) || (gamma > kMaxGamma)) {
        gamma = kInverseGamma;
        png_set_gAMA(png, info, gamma);
      }
      png_set_gamma(png, kDefaultGamma, gamma);
    } else {
      png_set_gamma(png, kDefaultGamma, kInverseGamma);
    }
  }

  // Tell libpng to send us rows for interlaced pngs.
  if (interlace_type == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png);

  // Update our info now (so we can get color channel info).
  png_read_update_info(png, info);

  int channels = png_get_channels(png, info);
  DCHECK(channels == 3 || channels == 4);
  has_alpha_channel_ = (channels == 4);
}

void PNGImageDecoder::RowAvailable(unsigned char* row_buffer,
                                   unsigned row_index,
                                   int) {
  if (current_frame_ >= frame_buffer_cache_.size())
    return;

  ImageFrame& buffer = frame_buffer_cache_[current_frame_];
  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    png_structp png = reader_->PngPtr();
    if (!InitFrameBuffer(current_frame_)) {
      longjmp(JMPBUF(png), 1);
      return;
    }

    DCHECK_EQ(ImageFrame::kFramePartial, buffer.GetStatus());

    if (PNG_INTERLACE_ADAM7 ==
        png_get_interlace_type(png, reader_->InfoPtr())) {
      unsigned color_channels = has_alpha_channel_ ? 4 : 3;
      reader_->CreateInterlaceBuffer(color_channels * Size().Area());
      if (!reader_->InterlaceBuffer()) {
        longjmp(JMPBUF(png), 1);
        return;
      }
    }

    current_buffer_saw_alpha_ = false;
  }

  const IntRect& frame_rect = buffer.OriginalFrameRect();
  DCHECK(IntRect(IntPoint(), Size()).Contains(frame_rect));

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
  if (!row_buffer)
    return;

  DCHECK_GT(frame_rect.Height(), 0);
  if (row_index >= static_cast<unsigned>(frame_rect.Height()))
    return;

  int y = row_index + frame_rect.Y();
  if (y < 0)
    return;
  DCHECK_LT(y, Size().Height());

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

  bool has_alpha = has_alpha_channel_;
  png_bytep row = row_buffer;

  if (png_bytep interlace_buffer = reader_->InterlaceBuffer()) {
    unsigned color_channels = has_alpha ? 4 : 3;
    row = interlace_buffer + (row_index * color_channels * Size().Width());
    png_progressive_combine_row(reader_->PngPtr(), row, row_buffer);
  }

  // Write the decoded row pixels to the frame buffer. The repetitive
  // form of the row write loops is for speed.
  ImageFrame::PixelData* const dst_row = buffer.GetAddr(frame_rect.X(), y);
  const int width = frame_rect.Width();

  png_bytep src_ptr = row;
  if (has_alpha) {
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
    if (SkColorSpaceXform* xform = ColorTransform()) {
      SkColorSpaceXform::ColorFormat color_format =
          SkColorSpaceXform::kRGBA_8888_ColorFormat;
      xform->apply(color_format, dst_row, color_format, src_ptr, width,
                   kUnpremul_SkAlphaType);
      src_ptr = png_bytep(dst_row);
    }

    unsigned alpha_mask = 255;
    if (frame_buffer_cache_[current_frame_].GetAlphaBlendSource() ==
        ImageFrame::kBlendAtopBgcolor) {
      if (buffer.PremultiplyAlpha()) {
        for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
             dst_pixel++, src_ptr += 4) {
          ImageFrame::SetRGBAPremultiply(dst_pixel, src_ptr[0], src_ptr[1],
                                         src_ptr[2], src_ptr[3]);
          alpha_mask &= src_ptr[3];
        }
      } else {
        for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
             dst_pixel++, src_ptr += 4) {
          ImageFrame::SetRGBARaw(dst_pixel, src_ptr[0], src_ptr[1], src_ptr[2],
                                 src_ptr[3]);
          alpha_mask &= src_ptr[3];
        }
      }
    } else {
      // Now, the blend method is ImageFrame::BlendAtopPreviousFrame. Since the
      // frame data of the previous frame is copied at InitFrameBuffer, we can
      // blend the pixel of this frame, stored in |src_ptr|, over the previous
      // pixel stored in |dst_pixel|.
      if (buffer.PremultiplyAlpha()) {
        for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
             dst_pixel++, src_ptr += 4) {
          ImageFrame::BlendRGBAPremultiplied(dst_pixel, src_ptr[0], src_ptr[1],
                                             src_ptr[2], src_ptr[3]);
          alpha_mask &= src_ptr[3];
        }
      } else {
        for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
             dst_pixel++, src_ptr += 4) {
          ImageFrame::BlendRGBARaw(dst_pixel, src_ptr[0], src_ptr[1],
                                   src_ptr[2], src_ptr[3]);
          alpha_mask &= src_ptr[3];
        }
      }
    }

    if (alpha_mask != 255)
      current_buffer_saw_alpha_ = true;

  } else {
    for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
         src_ptr += 3, ++dst_pixel) {
      ImageFrame::SetRGBARaw(dst_pixel, src_ptr[0], src_ptr[1], src_ptr[2],
                             255);
    }

    // We'll apply the color space xform to opaque pixels after they have been
    // written to the ImageFrame, purely because SkColorSpaceXform supports
    // RGBA (and not RGB).
    if (SkColorSpaceXform* xform = ColorTransform()) {
      xform->apply(XformColorFormat(), dst_row, XformColorFormat(), dst_row,
                   width, kOpaque_SkAlphaType);
    }
  }

  buffer.SetPixelsChanged(true);
}

void PNGImageDecoder::FrameComplete() {
  if (current_frame_ >= frame_buffer_cache_.size())
    return;

  if (reader_->InterlaceBuffer())
    reader_->ClearInterlaceBuffer();

  ImageFrame& buffer = frame_buffer_cache_[current_frame_];
  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    longjmp(JMPBUF(reader_->PngPtr()), 1);
    return;
  }

  if (!current_buffer_saw_alpha_)
    CorrectAlphaWhenFrameBufferSawNoAlpha(current_frame_);

  buffer.SetStatus(ImageFrame::kFrameComplete);
}

bool PNGImageDecoder::FrameIsReceivedAtIndex(size_t index) const {
  if (!IsDecodedSizeAvailable())
    return false;

  DCHECK(!Failed() && reader_);

  // For non-animated images, return ImageDecoder::FrameIsReceivedAtIndex.
  // This matches the behavior of WEBPImageDecoder.
  if (reader_->ParseCompleted() && reader_->FrameCount() == 1)
    return ImageDecoder::FrameIsReceivedAtIndex(index);

  return reader_->FrameIsReceivedAtIndex(index);
}

float PNGImageDecoder::FrameDurationAtIndex(size_t index) const {
  if (index < frame_buffer_cache_.size())
    return frame_buffer_cache_[index].Duration();
  return 0;
}

}  // namespace blink
