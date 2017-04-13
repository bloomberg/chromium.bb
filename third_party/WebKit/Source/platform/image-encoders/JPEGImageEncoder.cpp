/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#include "platform/image-encoders/JPEGImageEncoder.h"

#include <memory>
#include "SkColorPriv.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/image-encoders/RGBAtoRGB.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"

extern "C" {
#include <setjmp.h>
#include <stdio.h>  // jpeglib.h needs stdio.h FILE
#include "jpeglib.h"
}

namespace blink {

void RGBAtoRGBScalar(const unsigned char* pixels,
                     unsigned pixel_count,
                     unsigned char* output) {
  // Per <canvas> spec, composite the input image pixels source-over on black.
  for (; pixel_count-- > 0; pixels += 4) {
    unsigned char alpha = pixels[3];
    if (alpha != 255) {
      *output++ = SkMulDiv255Round(pixels[0], alpha);
      *output++ = SkMulDiv255Round(pixels[1], alpha);
      *output++ = SkMulDiv255Round(pixels[2], alpha);
    } else {
      *output++ = pixels[0];
      *output++ = pixels[1];
      *output++ = pixels[2];
    }
  }
}

// TODO(cavalcantii): use regular macro, see https://crbug.com/673067.
#ifdef __ARM_NEON__
void RGBAtoRGBNeon(const unsigned char* input,
                   const unsigned pixel_count,
                   unsigned char* output) {
  const unsigned kPixelsPerLoad = 16;
  const unsigned kRgbaStep = kPixelsPerLoad * 4, kRgbStep = kPixelsPerLoad * 3;
  // Input registers.
  uint8x16x4_t rgba;
  // Output registers.
  uint8x16x3_t rgb;
  // Intermediate registers.
  uint8x8_t low, high;
  uint8x16_t result;
  unsigned counter;
  auto transform_color = [&](size_t channel) {
    // Extracts the low/high part of the 128 bits.
    low = vget_low_u8(rgba.val[channel]);
    high = vget_high_u8(rgba.val[channel]);
    // Scale the color and combine.
    uint16x8_t temp = vmull_u8(low, vget_low_u8(rgba.val[3]));
    low = vraddhn_u16(temp, vrshrq_n_u16(temp, 8));
    temp = vmull_u8(high, vget_high_u8(rgba.val[3]));
    high = vraddhn_u16(temp, vrshrq_n_u16(temp, 8));
    result = vcombine_u8(low, high);
    // Write back the channel to a 128 bits register.
    rgb.val[channel] = result;
  };

  for (counter = 0; counter + kPixelsPerLoad <= pixel_count;
       counter += kPixelsPerLoad) {
    // Reads 16 pixels at once, each color channel in a different
    // 128 bits register.
    rgba = vld4q_u8(input);

    transform_color(0);
    transform_color(1);
    transform_color(2);

    // Write back (interleaved) results to output.
    vst3q_u8(output, rgb);

    // Advance to next elements (could be avoided loading register with
    // increment after i.e. "vld4 {vector}, [r1]!").
    input += kRgbaStep;
    output += kRgbStep;
  }

  // Handle the tail elements.
  unsigned remaining = pixel_count;
  remaining -= counter;
  if (remaining != 0) {
    RGBAtoRGBScalar(input, remaining, output);
  }
}
#endif

struct JPEGOutputBuffer : public jpeg_destination_mgr {
  DISALLOW_NEW();
  Vector<unsigned char>* output;
  Vector<unsigned char> buffer;
};

class JPEGImageEncoderStateImpl final : public JPEGImageEncoderState {
 public:
  JPEGImageEncoderStateImpl() {}
  ~JPEGImageEncoderStateImpl() override {
    jpeg_destroy_compress(&cinfo_);
    cinfo_.client_data = 0;
  }
  JPEGOutputBuffer* OutputBuffer() { return &output_buffer_; }
  jpeg_compress_struct* Cinfo() { return &cinfo_; }
  jpeg_error_mgr* GetError() { return &error_; }

 private:
  JPEGOutputBuffer output_buffer_;
  jpeg_compress_struct cinfo_;
  jpeg_error_mgr error_;
};

static void PrepareOutput(j_compress_ptr cinfo) {
  JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
  const size_t kInternalBufferSize = 8192;
  out->buffer.Resize(kInternalBufferSize);
  out->next_output_byte = out->buffer.Data();
  out->free_in_buffer = out->buffer.size();
}

static boolean WriteOutput(j_compress_ptr cinfo) {
  JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
  out->output->Append(out->buffer.Data(), out->buffer.size());
  out->next_output_byte = out->buffer.Data();
  out->free_in_buffer = out->buffer.size();
  return TRUE;
}

static void FinishOutput(j_compress_ptr cinfo) {
  JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
  const size_t size = out->buffer.size() - out->free_in_buffer;
  out->output->Append(out->buffer.Data(), size);
}

static void HandleError(j_common_ptr common) {
  jmp_buf* jump_buffer_ptr = static_cast<jmp_buf*>(common->client_data);
  longjmp(*jump_buffer_ptr, -1);
}

static void DisableSubsamplingForHighQuality(jpeg_compress_struct* cinfo,
                                             int quality) {
  if (quality < 100)
    return;

  for (int i = 0; i < MAX_COMPONENTS; ++i) {
    cinfo->comp_info[i].h_samp_factor = 1;
    cinfo->comp_info[i].v_samp_factor = 1;
  }
}

#define SET_JUMP_BUFFER(jpeg_compress_struct_ptr, what_to_return) \
  jmp_buf jump_buffer;                                            \
  jpeg_compress_struct_ptr->client_data = &jump_buffer;           \
  if (setjmp(jump_buffer)) {                                      \
    return what_to_return;                                        \
  }

std::unique_ptr<JPEGImageEncoderState> JPEGImageEncoderState::Create(
    const IntSize& image_size,
    const double& quality,
    Vector<unsigned char>* output) {
  if (image_size.Width() <= 0 || image_size.Height() <= 0)
    return nullptr;

  std::unique_ptr<JPEGImageEncoderStateImpl> encoder_state =
      WTF::MakeUnique<JPEGImageEncoderStateImpl>();

  jpeg_compress_struct* cinfo = encoder_state->Cinfo();
  jpeg_error_mgr* error = encoder_state->GetError();
  cinfo->err = jpeg_std_error(error);
  error->error_exit = HandleError;

  SET_JUMP_BUFFER(cinfo, nullptr);

  JPEGOutputBuffer* destination = encoder_state->OutputBuffer();
  destination->output = output;

  jpeg_create_compress(cinfo);
  cinfo->dest = destination;
  cinfo->dest->init_destination = PrepareOutput;
  cinfo->dest->empty_output_buffer = WriteOutput;
  cinfo->dest->term_destination = FinishOutput;

  cinfo->image_height = image_size.Height();
  cinfo->image_width = image_size.Width();
  cinfo->in_color_space = JCS_RGB;
  cinfo->input_components = 3;

  jpeg_set_defaults(cinfo);
  int compression_quality =
      JPEGImageEncoder::ComputeCompressionQuality(quality);
  jpeg_set_quality(cinfo, compression_quality, TRUE);
  DisableSubsamplingForHighQuality(cinfo, compression_quality);
  jpeg_start_compress(cinfo, TRUE);

  cinfo->client_data = 0;
  return std::move(encoder_state);
}

int JPEGImageEncoder::ComputeCompressionQuality(const double& quality) {
  int compression_quality = JPEGImageEncoder::kDefaultCompressionQuality;
  if (quality >= 0.0 && quality <= 1.0)
    compression_quality = static_cast<int>(quality * 100 + 0.5);
  return compression_quality;
}

int JPEGImageEncoder::ProgressiveEncodeRowsJpegHelper(
    JPEGImageEncoderState* encoder_state,
    unsigned char* data,
    int current_rows_completed,
    const double slack_before_deadline,
    double deadline_seconds) {
  JPEGImageEncoderStateImpl* encoder_state_impl =
      static_cast<JPEGImageEncoderStateImpl*>(encoder_state);
  Vector<JSAMPLE> row(encoder_state_impl->Cinfo()->image_width *
                      encoder_state_impl->Cinfo()->input_components);
  SET_JUMP_BUFFER(encoder_state_impl->Cinfo(), kProgressiveEncodeFailed);

  const size_t pixel_row_stride = encoder_state_impl->Cinfo()->image_width * 4;
  unsigned char* pixels = data + pixel_row_stride * current_rows_completed;

  while (encoder_state_impl->Cinfo()->next_scanline <
         encoder_state_impl->Cinfo()->image_height) {
    JSAMPLE* row_data = row.Data();
    RGBAtoRGB(pixels, encoder_state_impl->Cinfo()->image_width, row_data);
    jpeg_write_scanlines(encoder_state_impl->Cinfo(), &row_data, 1);
    pixels += pixel_row_stride;
    current_rows_completed++;

    if (deadline_seconds - slack_before_deadline -
            MonotonicallyIncreasingTime() <=
        0) {
      return current_rows_completed;
    }
  }

  jpeg_finish_compress(encoder_state_impl->Cinfo());
  return current_rows_completed;
}

bool JPEGImageEncoder::EncodeWithPreInitializedState(
    std::unique_ptr<JPEGImageEncoderState> encoder_state,
    const unsigned char* input_pixels,
    int num_rows_completed) {
  JPEGImageEncoderStateImpl* encoder_state_impl =
      static_cast<JPEGImageEncoderStateImpl*>(encoder_state.get());

  Vector<JSAMPLE> row;
  row.Resize(encoder_state_impl->Cinfo()->image_width *
             encoder_state_impl->Cinfo()->input_components);

  SET_JUMP_BUFFER(encoder_state_impl->Cinfo(), false);

  const size_t pixel_row_stride = encoder_state_impl->Cinfo()->image_width * 4;
  unsigned char* pixels = const_cast<unsigned char*>(input_pixels) +
                          pixel_row_stride * num_rows_completed;
  while (encoder_state_impl->Cinfo()->next_scanline <
         encoder_state_impl->Cinfo()->image_height) {
    JSAMPLE* row_data = row.Data();
    RGBAtoRGB(pixels, encoder_state_impl->Cinfo()->image_width, row_data);
    jpeg_write_scanlines(encoder_state_impl->Cinfo(), &row_data, 1);
    pixels += pixel_row_stride;
  }

  jpeg_finish_compress(encoder_state_impl->Cinfo());
  return true;
}

bool JPEGImageEncoder::Encode(const ImageDataBuffer& image_data,
                              const double& quality,
                              Vector<unsigned char>* output) {
  if (!image_data.Pixels())
    return false;

  std::unique_ptr<JPEGImageEncoderState> encoder_state =
      JPEGImageEncoderState::Create(image_data.size(), quality, output);
  if (!encoder_state)
    return false;

  return JPEGImageEncoder::EncodeWithPreInitializedState(
      std::move(encoder_state), image_data.Pixels());
}

}  // namespace blink
