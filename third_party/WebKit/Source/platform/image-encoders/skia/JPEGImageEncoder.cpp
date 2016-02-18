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

#include "platform/image-encoders/skia/JPEGImageEncoder.h"

#include "SkColorPriv.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"

extern "C" {
#include <setjmp.h>
#include <stdio.h> // jpeglib.h needs stdio.h FILE
#include "jpeglib.h"
}

namespace blink {

struct JPEGOutputBuffer : public jpeg_destination_mgr {
    USING_FAST_MALLOC(JPEGOutputBuffer);
    WTF_MAKE_NONCOPYABLE(JPEGOutputBuffer);
public:
    JPEGOutputBuffer() : jpeg_destination_mgr() {}
    Vector<unsigned char>* output;
    Vector<unsigned char> buffer;
};

class JPEGImageEncoderStateImpl : public JPEGImageEncoderState {
public:
    JPEGImageEncoderStateImpl(jpeg_compress_struct* jpeg, JPEGOutputBuffer* destination, jpeg_error_mgr* error)
        : m_jpeg(jpeg)
        , m_destination(destination)
        , m_error(error) {}
    ~JPEGImageEncoderStateImpl() override;
    jpeg_compress_struct* jpeg() { ASSERT(m_jpeg); return m_jpeg; }
    JPEGOutputBuffer* outputBuffer() { return m_destination; }
    jpeg_error_mgr* error() { return m_error; }
private:
    jpeg_compress_struct* m_jpeg;
    JPEGOutputBuffer* m_destination;
    jpeg_error_mgr* m_error;
};

static void prepareOutput(j_compress_ptr cinfo)
{
    JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
    const size_t internalBufferSize = 8192;
    out->buffer.resize(internalBufferSize);
    out->next_output_byte = out->buffer.data();
    out->free_in_buffer = out->buffer.size();
}

static boolean writeOutput(j_compress_ptr cinfo)
{
    JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
    out->output->append(out->buffer.data(), out->buffer.size());
    out->next_output_byte = out->buffer.data();
    out->free_in_buffer = out->buffer.size();
    return TRUE;
}

static void finishOutput(j_compress_ptr cinfo)
{
    JPEGOutputBuffer* out = static_cast<JPEGOutputBuffer*>(cinfo->dest);
    const size_t size = out->buffer.size() - out->free_in_buffer;
    out->output->append(out->buffer.data(), size);
}

static void handleError(j_common_ptr common)
{
    jmp_buf* jumpBufferPtr = static_cast<jmp_buf*>(common->client_data);
    longjmp(*jumpBufferPtr, -1);
}

static void RGBAtoRGB(const unsigned char* pixels, unsigned pixelCount, unsigned char* output)
{
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

static void disableSubsamplingForHighQuality(jpeg_compress_struct* cinfo, int quality)
{
    if (quality < 100)
        return;

    for (int i = 0; i < MAX_COMPONENTS; ++i) {
        cinfo->comp_info[i].h_samp_factor = 1;
        cinfo->comp_info[i].v_samp_factor = 1;
    }
}

PassOwnPtr<JPEGImageEncoderState> JPEGImageEncoderState::create(const IntSize& imageSize, const double& quality, Vector<unsigned char>* output)
{
    if (imageSize.width() <= 0 || imageSize.height() <= 0)
        return nullptr;

    int compressionQuality = JPEGImageEncoder::computeCompressionQuality(quality);
    JPEGOutputBuffer* destination = new JPEGOutputBuffer;
    destination->output = output;

    jpeg_compress_struct* cinfo = new jpeg_compress_struct;
    jpeg_error_mgr* error = new jpeg_error_mgr;

    cinfo->err = jpeg_std_error(error);
    error->error_exit = handleError;
    jmp_buf jumpBuffer;
    cinfo->client_data = &jumpBuffer;

    if (setjmp(jumpBuffer)) {
        jpeg_destroy_compress(cinfo);
        return nullptr;
    }

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
    jpeg_set_quality(cinfo, compressionQuality, TRUE);
    disableSubsamplingForHighQuality(cinfo, compressionQuality);
    jpeg_start_compress(cinfo, TRUE);

    return adoptPtr(new JPEGImageEncoderStateImpl(cinfo, destination, error));
}

JPEGImageEncoderStateImpl::~JPEGImageEncoderStateImpl()
{
    jpeg_destroy_compress(m_jpeg);
}

int JPEGImageEncoder::computeCompressionQuality(const double& quality)
{
    int compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
    if (quality >= 0.0 && quality <= 1.0)
        compressionQuality = static_cast<int>(quality * 100 + 0.5);
    return compressionQuality;
}

void JPEGImageEncoder::encodeWithPreInitializedState(JPEGImageEncoderState* encoderState, const unsigned char* inputPixels)
{
    JPEGImageEncoderStateImpl* encoderStateImpl = static_cast<JPEGImageEncoderStateImpl *>(encoderState);

    Vector<JSAMPLE> row;
    unsigned char* pixels = const_cast<unsigned char*>(inputPixels);

    row.resize(encoderStateImpl->jpeg()->image_width * encoderStateImpl->jpeg()->input_components);

    const size_t pixelRowStride = encoderStateImpl->jpeg()->image_width * 4;
    while (encoderStateImpl->jpeg()->next_scanline < encoderStateImpl->jpeg()->image_height) {
        JSAMPLE* rowData = row.data();
        RGBAtoRGB(pixels, encoderStateImpl->jpeg()->image_width, rowData);
        jpeg_write_scanlines(encoderStateImpl->jpeg(), &rowData, 1);
        pixels += pixelRowStride;
    }

    jpeg_finish_compress(encoderStateImpl->jpeg());
}

bool JPEGImageEncoder::encode(const ImageDataBuffer& imageData, const double& quality, Vector<unsigned char>* output)
{
    if (!imageData.pixels())
        return false;
    IntSize imageSize(imageData.width(), imageData.height());
    const unsigned char* inputPixels = imageData.pixels();

    OwnPtr<JPEGImageEncoderState> encoderState = JPEGImageEncoderState::create(imageSize, quality, output);
    if (!encoderState.get())
        return false;

    JPEGImageEncoder::encodeWithPreInitializedState(encoderState.get(), inputPixels);
    return true;
}

} // namespace blink
