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

#include "SkColorPriv.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/image-encoders/RGBAtoRGB.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

extern "C" {
#include <setjmp.h>
#include <stdio.h>  // jpeglib.h needs stdio.h FILE
#include "jpeglib.h"
}

namespace blink {

void RGBAtoRGBScalar(const unsigned char* pixels,
                     unsigned pixelCount,
                     unsigned char* output) {
  // Per <canvas> spec, composite the input image pixels source-over on black.
  for (; pixelCount-- > 0; pixels += 4) {
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
                   const unsigned pixelCount,
                   unsigned char* output) {
  const unsigned pixelsPerLoad = 16;
  const unsigned rgbaStep = pixelsPerLoad * 4, rgbStep = pixelsPerLoad * 3;
  // Input registers.
  uint8x16x4_t rgba;
  // Output registers.
  uint8x16x3_t rgb;
  // Intermediate registers.
  uint8x8_t low, high;
  uint8x16_t result;
  unsigned counter;
  auto transformColor = [&](size_t channel) {
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

  for (counter = 0; counter + pixelsPerLoad <= pixelCount;
       counter += pixelsPerLoad) {
    // Reads 16 pixels at once, each color channel in a different
    // 128 bits register.
    rgba = vld4q_u8(input);

    transformColor(0);
    transformColor(1);
    transformColor(2);

    // Write back (interleaved) results to output.
    vst3q_u8(output, rgb);

    // Advance to next elements (could be avoided loading register with
    // increment after i.e. "vld4 {vector}, [r1]!").
    input += rgbaStep;
    output += rgbStep;
  }

  // Handle the tail elements.
  unsigned remaining = pixelCount;
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
    jpeg_destroy_compress(&m_cinfo);
    m_cinfo.client_data = 0;
  }
  JPEGOutputBuffer* outputBuffer() { return &m_outputBuffer; }
  jpeg_compress_struct* cinfo() { return &m_cinfo; }
  jpeg_error_mgr* error() { return &m_error; }

 private:
  JPEGOutputBuffer m_outputBuffer;
  jpeg_compress_struct m_cinfo;
  jpeg_error_mgr m_error;
};

static void prepareOutput(j_compress_ptr cinfo) {
  JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
  const size_t internalBufferSize = 8192;
  out->buffer.resize(internalBufferSize);
  out->next_output_byte = out->buffer.data();
  out->free_in_buffer = out->buffer.size();
}

static boolean writeOutput(j_compress_ptr cinfo) {
  JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
  out->output->append(out->buffer.data(), out->buffer.size());
  out->next_output_byte = out->buffer.data();
  out->free_in_buffer = out->buffer.size();
  return TRUE;
}

static void finishOutput(j_compress_ptr cinfo) {
  JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
  const size_t size = out->buffer.size() - out->free_in_buffer;
  out->output->append(out->buffer.data(), size);
}

static void handleError(j_common_ptr common) {
  jmp_buf* jumpBufferPtr = static_cast<jmp_buf*>(common->client_data);
  longjmp(*jumpBufferPtr, -1);
}

static void disableSubsamplingForHighQuality(jpeg_compress_struct* cinfo,
                                             int quality) {
  if (quality < 100)
    return;

  for (int i = 0; i < MAX_COMPONENTS; ++i) {
    cinfo->comp_info[i].h_samp_factor = 1;
    cinfo->comp_info[i].v_samp_factor = 1;
  }
}

#define SET_JUMP_BUFFER(jpeg_compress_struct_ptr, what_to_return) \
  jmp_buf jumpBuffer;                                             \
  jpeg_compress_struct_ptr->client_data = &jumpBuffer;            \
  if (setjmp(jumpBuffer)) {                                       \
    return what_to_return;                                        \
  }

std::unique_ptr<JPEGImageEncoderState> JPEGImageEncoderState::create(
    const IntSize& imageSize,
    const double& quality,
    Vector<unsigned char>* output) {
  if (imageSize.width() <= 0 || imageSize.height() <= 0)
    return nullptr;

  std::unique_ptr<JPEGImageEncoderStateImpl> encoderState =
      WTF::makeUnique<JPEGImageEncoderStateImpl>();

  jpeg_compress_struct* cinfo = encoderState->cinfo();
  jpeg_error_mgr* error = encoderState->error();
  cinfo->err = jpeg_std_error(error);
  error->error_exit = handleError;

  SET_JUMP_BUFFER(cinfo, nullptr);

  JPEGOutputBuffer* destination = encoderState->outputBuffer();
  destination->output = output;

  jpeg_create_compress(cinfo);
  cinfo->dest = destination;
  cinfo->dest->init_destination = prepareOutput;
  cinfo->dest->empty_output_buffer = writeOutput;
  cinfo->dest->term_destination = finishOutput;

  cinfo->image_height = imageSize.height();
  cinfo->image_width = imageSize.width();
  cinfo->in_color_space = JCS_RGB;
  cinfo->input_components = 3;

  jpeg_set_defaults(cinfo);
  int compressionQuality = JPEGImageEncoder::computeCompressionQuality(quality);
  jpeg_set_quality(cinfo, compressionQuality, TRUE);
  disableSubsamplingForHighQuality(cinfo, compressionQuality);
  jpeg_start_compress(cinfo, TRUE);

  cinfo->client_data = 0;
  return std::move(encoderState);
}

int JPEGImageEncoder::computeCompressionQuality(const double& quality) {
  int compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
  if (quality >= 0.0 && quality <= 1.0)
    compressionQuality = static_cast<int>(quality * 100 + 0.5);
  return compressionQuality;
}

int JPEGImageEncoder::progressiveEncodeRowsJpegHelper(
    JPEGImageEncoderState* encoderState,
    unsigned char* data,
    int currentRowsCompleted,
    const double SlackBeforeDeadline,
    double deadlineSeconds) {
  JPEGImageEncoderStateImpl* encoderStateImpl =
      static_cast<JPEGImageEncoderStateImpl*>(encoderState);
  Vector<JSAMPLE> row(encoderStateImpl->cinfo()->image_width *
                      encoderStateImpl->cinfo()->input_components);
  SET_JUMP_BUFFER(encoderStateImpl->cinfo(), ProgressiveEncodeFailed);

  const size_t pixelRowStride = encoderStateImpl->cinfo()->image_width * 4;
  unsigned char* pixels = data + pixelRowStride * currentRowsCompleted;

  while (encoderStateImpl->cinfo()->next_scanline <
         encoderStateImpl->cinfo()->image_height) {
    JSAMPLE* rowData = row.data();
    RGBAtoRGB(pixels, encoderStateImpl->cinfo()->image_width, rowData);
    jpeg_write_scanlines(encoderStateImpl->cinfo(), &rowData, 1);
    pixels += pixelRowStride;
    currentRowsCompleted++;

    if (deadlineSeconds - SlackBeforeDeadline - monotonicallyIncreasingTime() <=
        0) {
      return currentRowsCompleted;
    }
  }

  jpeg_finish_compress(encoderStateImpl->cinfo());
  return currentRowsCompleted;
}

bool JPEGImageEncoder::encodeWithPreInitializedState(
    std::unique_ptr<JPEGImageEncoderState> encoderState,
    const unsigned char* inputPixels,
    int numRowsCompleted) {
  JPEGImageEncoderStateImpl* encoderStateImpl =
      static_cast<JPEGImageEncoderStateImpl*>(encoderState.get());

  Vector<JSAMPLE> row;
  row.resize(encoderStateImpl->cinfo()->image_width *
             encoderStateImpl->cinfo()->input_components);

  SET_JUMP_BUFFER(encoderStateImpl->cinfo(), false);

  const size_t pixelRowStride = encoderStateImpl->cinfo()->image_width * 4;
  unsigned char* pixels = const_cast<unsigned char*>(inputPixels) +
                          pixelRowStride * numRowsCompleted;
  while (encoderStateImpl->cinfo()->next_scanline <
         encoderStateImpl->cinfo()->image_height) {
    JSAMPLE* rowData = row.data();
    RGBAtoRGB(pixels, encoderStateImpl->cinfo()->image_width, rowData);
    jpeg_write_scanlines(encoderStateImpl->cinfo(), &rowData, 1);
    pixels += pixelRowStride;
  }

  jpeg_finish_compress(encoderStateImpl->cinfo());
  return true;
}

bool JPEGImageEncoder::encode(const ImageDataBuffer& imageData,
                              const double& quality,
                              Vector<unsigned char>* output) {
  if (!imageData.pixels())
    return false;

  std::unique_ptr<JPEGImageEncoderState> encoderState =
      JPEGImageEncoderState::create(imageData.size(), quality, output);
  if (!encoderState)
    return false;

  return JPEGImageEncoder::encodeWithPreInitializedState(
      std::move(encoderState), imageData.pixels());
}

}  // namespace blink
