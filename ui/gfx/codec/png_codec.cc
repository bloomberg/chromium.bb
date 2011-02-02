// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/codec/png_codec.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/skia/include/core/SkColorPriv.h"

extern "C" {
#if defined(USE_SYSTEM_LIBPNG)
#include <png.h>
#else
#include "third_party/libpng/png.h"
#endif
}

namespace gfx {

namespace {

// Converts BGRA->RGBA and RGBA->BGRA.
void ConvertBetweenBGRAandRGBA(const unsigned char* input, int pixel_width,
                               unsigned char* output, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &input[x * 4];
    unsigned char* pixel_out = &output[x * 4];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = pixel_in[3];
  }
}

void ConvertRGBAtoRGB(const unsigned char* rgba, int pixel_width,
                      unsigned char* rgb, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgba[x * 4];
    unsigned char* pixel_out = &rgb[x * 3];
    pixel_out[0] = pixel_in[0];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[2];
  }
}

void ConvertRGBtoSkia(const unsigned char* rgb, int pixel_width,
                      unsigned char* rgba, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgb[x * 3];
    uint32_t* pixel_out = reinterpret_cast<uint32_t*>(&rgba[x * 4]);
    *pixel_out = SkPackARGB32(0xFF, pixel_in[0], pixel_in[1], pixel_in[2]);
  }
}

void ConvertRGBAtoSkia(const unsigned char* rgb, int pixel_width,
                       unsigned char* rgba, bool* is_opaque) {
  int total_length = pixel_width * 4;
  for (int x = 0; x < total_length; x += 4) {
    const unsigned char* pixel_in = &rgb[x];
    uint32_t* pixel_out = reinterpret_cast<uint32_t*>(&rgba[x]);

    unsigned char alpha = pixel_in[3];
    if (alpha != 255) {
      *is_opaque = false;
      *pixel_out = SkPreMultiplyARGB(alpha,
           pixel_in[0], pixel_in[1], pixel_in[2]);
    } else {
      *pixel_out = SkPackARGB32(alpha,
           pixel_in[0], pixel_in[1], pixel_in[2]);
    }
  }
}

void ConvertSkiatoRGB(const unsigned char* skia, int pixel_width,
                      unsigned char* rgb, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const uint32_t pixel_in = *reinterpret_cast<const uint32_t*>(&skia[x * 4]);
    unsigned char* pixel_out = &rgb[x * 3];

    int alpha = SkGetPackedA32(pixel_in);
    if (alpha != 0 && alpha != 255) {
      SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(pixel_in);
      pixel_out[0] = SkColorGetR(unmultiplied);
      pixel_out[1] = SkColorGetG(unmultiplied);
      pixel_out[2] = SkColorGetB(unmultiplied);
    } else {
      pixel_out[0] = SkGetPackedR32(pixel_in);
      pixel_out[1] = SkGetPackedG32(pixel_in);
      pixel_out[2] = SkGetPackedB32(pixel_in);
    }
  }
}

void ConvertSkiatoRGBA(const unsigned char* skia, int pixel_width,
                       unsigned char* rgba, bool* is_opaque) {
  int total_length = pixel_width * 4;
  for (int i = 0; i < total_length; i += 4) {
    const uint32_t pixel_in = *reinterpret_cast<const uint32_t*>(&skia[i]);

    // Pack the components here.
    int alpha = SkGetPackedA32(pixel_in);
    if (alpha != 0 && alpha != 255) {
      SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(pixel_in);
      rgba[i + 0] = SkColorGetR(unmultiplied);
      rgba[i + 1] = SkColorGetG(unmultiplied);
      rgba[i + 2] = SkColorGetB(unmultiplied);
      rgba[i + 3] = alpha;
    } else {
      rgba[i + 0] = SkGetPackedR32(pixel_in);
      rgba[i + 1] = SkGetPackedG32(pixel_in);
      rgba[i + 2] = SkGetPackedB32(pixel_in);
      rgba[i + 3] = alpha;
    }
  }
}

}  // namespace

// Decoder --------------------------------------------------------------------
//
// This code is based on WebKit libpng interface (PNGImageDecoder), which is
// in turn based on the Mozilla png decoder.

namespace {

// Gamma constants: We assume we're on Windows which uses a gamma of 2.2.
const double kMaxGamma = 21474.83;  // Maximum gamma accepted by png library.
const double kDefaultGamma = 2.2;
const double kInverseGamma = 1.0 / kDefaultGamma;

class PngDecoderState {
 public:
  // Output is a vector<unsigned char>.
  PngDecoderState(PNGCodec::ColorFormat ofmt, std::vector<unsigned char>* o)
      : output_format(ofmt),
        output_channels(0),
        bitmap(NULL),
        is_opaque(true),
        output(o),
        row_converter(NULL),
        width(0),
        height(0),
        done(false) {
  }

  // Output is an SkBitmap.
  explicit PngDecoderState(SkBitmap* skbitmap)
      : output_format(PNGCodec::FORMAT_SkBitmap),
        output_channels(0),
        bitmap(skbitmap),
        is_opaque(true),
        output(NULL),
        row_converter(NULL),
        width(0),
        height(0),
        done(false) {
  }

  PNGCodec::ColorFormat output_format;
  int output_channels;

  // An incoming SkBitmap to write to. If NULL, we write to output instead.
  SkBitmap* bitmap;

  // Used during the reading of an SkBitmap. Defaults to true until we see a
  // pixel with anything other than an alpha of 255.
  bool is_opaque;

  // The other way to decode output, where we write into an intermediary buffer
  // instead of directly to an SkBitmap.
  std::vector<unsigned char>* output;

  // Called to convert a row from the library to the correct output format.
  // When NULL, no conversion is necessary.
  void (*row_converter)(const unsigned char* in, int w, unsigned char* out,
                        bool* is_opaque);

  // Size of the image, set in the info callback.
  int width;
  int height;

  // Set to true when we've found the end of the data.
  bool done;

 private:
  DISALLOW_COPY_AND_ASSIGN(PngDecoderState);
};

void ConvertRGBtoRGBA(const unsigned char* rgb, int pixel_width,
                      unsigned char* rgba, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgb[x * 3];
    unsigned char* pixel_out = &rgba[x * 4];
    pixel_out[0] = pixel_in[0];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[2];
    pixel_out[3] = 0xff;
  }
}

void ConvertRGBtoBGRA(const unsigned char* rgb, int pixel_width,
                      unsigned char* bgra, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgb[x * 3];
    unsigned char* pixel_out = &bgra[x * 4];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = 0xff;
  }
}

// Called when the png header has been read. This code is based on the WebKit
// PNGImageDecoder
void DecodeInfoCallback(png_struct* png_ptr, png_info* info_ptr) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  int bit_depth, color_type, interlace_type, compression_type;
  int filter_type, channels;
  png_uint_32 w, h;
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
               &interlace_type, &compression_type, &filter_type);

  // Bounds check. When the image is unreasonably big, we'll error out and
  // end up back at the setjmp call when we set up decoding.  "Unreasonably big"
  // means "big enough that w * h * 32bpp might overflow an int"; we choose this
  // threshold to match WebKit and because a number of places in code assume
  // that an image's size (in bytes) fits in a (signed) int.
  unsigned long long total_size =
      static_cast<unsigned long long>(w) * static_cast<unsigned long long>(h);
  if (total_size > ((1 << 29) - 1))
    longjmp(png_jmpbuf(png_ptr), 1);
  state->width = static_cast<int>(w);
  state->height = static_cast<int>(h);

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    png_set_expand(png_ptr);

  // Transparency for paletted images.
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_expand(png_ptr);

  // Convert 16-bit to 8-bit.
  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  // Expand grayscale to RGB.
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  // Deal with gamma and keep it under our control.
  double gamma;
  if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
    if (gamma <= 0.0 || gamma > kMaxGamma) {
      gamma = kInverseGamma;
      png_set_gAMA(png_ptr, info_ptr, gamma);
    }
    png_set_gamma(png_ptr, kDefaultGamma, gamma);
  } else {
    png_set_gamma(png_ptr, kDefaultGamma, kInverseGamma);
  }

  // Tell libpng to send us rows for interlaced pngs.
  if (interlace_type == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png_ptr);

  // Update our info now
  png_read_update_info(png_ptr, info_ptr);
  channels = png_get_channels(png_ptr, info_ptr);

  // Pick our row format converter necessary for this data.
  if (channels == 3) {
    switch (state->output_format) {
      case PNGCodec::FORMAT_RGB:
        state->row_converter = NULL;  // no conversion necessary
        state->output_channels = 3;
        break;
      case PNGCodec::FORMAT_RGBA:
        state->row_converter = &ConvertRGBtoRGBA;
        state->output_channels = 4;
        break;
      case PNGCodec::FORMAT_BGRA:
        state->row_converter = &ConvertRGBtoBGRA;
        state->output_channels = 4;
        break;
      case PNGCodec::FORMAT_SkBitmap:
        state->row_converter = &ConvertRGBtoSkia;
        state->output_channels = 4;
        break;
      default:
        NOTREACHED() << "Unknown output format";
        break;
    }
  } else if (channels == 4) {
    switch (state->output_format) {
      case PNGCodec::FORMAT_RGB:
        state->row_converter = &ConvertRGBAtoRGB;
        state->output_channels = 3;
        break;
      case PNGCodec::FORMAT_RGBA:
        state->row_converter = NULL;  // no conversion necessary
        state->output_channels = 4;
        break;
      case PNGCodec::FORMAT_BGRA:
        state->row_converter = &ConvertBetweenBGRAandRGBA;
        state->output_channels = 4;
        break;
      case PNGCodec::FORMAT_SkBitmap:
        state->row_converter = &ConvertRGBAtoSkia;
        state->output_channels = 4;
        break;
      default:
        NOTREACHED() << "Unknown output format";
        break;
    }
  } else {
    NOTREACHED() << "Unknown input channels";
    longjmp(png_jmpbuf(png_ptr), 1);
  }

  if (state->bitmap) {
    state->bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                             state->width, state->height);
    state->bitmap->allocPixels();
  } else if (state->output) {
    state->output->resize(
        state->width * state->output_channels * state->height);
  }
}

void DecodeRowCallback(png_struct* png_ptr, png_byte* new_row,
                       png_uint_32 row_num, int pass) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  DCHECK(pass == 0) << "We didn't turn on interlace handling, but libpng is "
                       "giving us interlaced data.";
  if (static_cast<int>(row_num) > state->height) {
    NOTREACHED() << "Invalid row";
    return;
  }

  unsigned char* base = NULL;
  if (state->bitmap)
    base = reinterpret_cast<unsigned char*>(state->bitmap->getAddr32(0, 0));
  else if (state->output)
    base = &state->output->front();

  unsigned char* dest = &base[state->width * state->output_channels * row_num];
  if (state->row_converter)
    state->row_converter(new_row, state->width, dest, &state->is_opaque);
  else
    memcpy(dest, new_row, state->width * state->output_channels);
}

void DecodeEndCallback(png_struct* png_ptr, png_info* info) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  // Mark the image as complete, this will tell the Decode function that we
  // have successfully found the end of the data.
  state->done = true;
}

// Automatically destroys the given read structs on destruction to make
// cleanup and error handling code cleaner.
class PngReadStructDestroyer {
 public:
  PngReadStructDestroyer(png_struct** ps, png_info** pi) : ps_(ps), pi_(pi) {
  }
  ~PngReadStructDestroyer() {
    png_destroy_read_struct(ps_, pi_, NULL);
  }
 private:
  png_struct** ps_;
  png_info** pi_;
};

bool BuildPNGStruct(const unsigned char* input, size_t input_size,
                    png_struct** png_ptr, png_info** info_ptr) {
  if (input_size < 8)
    return false;  // Input data too small to be a png

  // Have libpng check the signature, it likes the first 8 bytes.
  if (png_sig_cmp(const_cast<unsigned char*>(input), 0, 8) != 0)
    return false;

  *png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!*png_ptr)
    return false;

  *info_ptr = png_create_info_struct(*png_ptr);
  if (!*info_ptr) {
    png_destroy_read_struct(png_ptr, NULL, NULL);
    return false;
  }

  return true;
}

}  // namespace

// static
bool PNGCodec::Decode(const unsigned char* input, size_t input_size,
                      ColorFormat format, std::vector<unsigned char>* output,
                      int* w, int* h) {
  png_struct* png_ptr = NULL;
  png_info* info_ptr = NULL;
  if (!BuildPNGStruct(input, input_size, &png_ptr, &info_ptr))
    return false;

  PngReadStructDestroyer destroyer(&png_ptr, &info_ptr);
  if (setjmp(png_jmpbuf(png_ptr))) {
    // The destroyer will ensure that the structures are cleaned up in this
    // case, even though we may get here as a jump from random parts of the
    // PNG library called below.
    return false;
  }

  PngDecoderState state(format, output);

  png_set_progressive_read_fn(png_ptr, &state, &DecodeInfoCallback,
                              &DecodeRowCallback, &DecodeEndCallback);
  png_process_data(png_ptr,
                   info_ptr,
                   const_cast<unsigned char*>(input),
                   input_size);

  if (!state.done) {
    // Fed it all the data but the library didn't think we got all the data, so
    // this file must be truncated.
    output->clear();
    return false;
  }

  *w = state.width;
  *h = state.height;
  return true;
}

// static
bool PNGCodec::Decode(const unsigned char* input, size_t input_size,
                      SkBitmap* bitmap) {
  DCHECK(bitmap);
  png_struct* png_ptr = NULL;
  png_info* info_ptr = NULL;
  if (!BuildPNGStruct(input, input_size, &png_ptr, &info_ptr))
    return false;

  PngReadStructDestroyer destroyer(&png_ptr, &info_ptr);
  if (setjmp(png_jmpbuf(png_ptr))) {
    // The destroyer will ensure that the structures are cleaned up in this
    // case, even though we may get here as a jump from random parts of the
    // PNG library called below.
    return false;
  }

  PngDecoderState state(bitmap);

  png_set_progressive_read_fn(png_ptr, &state, &DecodeInfoCallback,
                              &DecodeRowCallback, &DecodeEndCallback);
  png_process_data(png_ptr,
                   info_ptr,
                   const_cast<unsigned char*>(input),
                   input_size);

  if (!state.done) {
    return false;
  }

  // Set the bitmap's opaqueness based on what we saw.
  bitmap->setIsOpaque(state.is_opaque);

  return true;
}

// static
SkBitmap* PNGCodec::CreateSkBitmapFromBGRAFormat(
    std::vector<unsigned char>& bgra, int width, int height) {
  SkBitmap* bitmap = new SkBitmap();
  bitmap->setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap->allocPixels();

  bool opaque = false;
  unsigned char* bitmap_data =
      reinterpret_cast<unsigned char*>(bitmap->getAddr32(0, 0));
  for (int i = width * height * 4 - 4; i >= 0; i -= 4) {
    unsigned char alpha = bgra[i + 3];
    if (!opaque && alpha != 255) {
      opaque = false;
    }
    bitmap_data[i + 3] = alpha;
    bitmap_data[i] = (bgra[i] * alpha) >> 8;
    bitmap_data[i + 1] = (bgra[i + 1] * alpha) >> 8;
    bitmap_data[i + 2] = (bgra[i + 2] * alpha) >> 8;
  }

  bitmap->setIsOpaque(opaque);
  return bitmap;
}

// Encoder --------------------------------------------------------------------
//
// This section of the code is based on nsPNGEncoder.cpp in Mozilla
// (Copyright 2005 Google Inc.)

namespace {

// Passed around as the io_ptr in the png structs so our callbacks know where
// to write data.
struct PngEncoderState {
  explicit PngEncoderState(std::vector<unsigned char>* o) : out(o) {}
  std::vector<unsigned char>* out;
};

// Called by libpng to flush its internal buffer to ours.
void EncoderWriteCallback(png_structp png, png_bytep data, png_size_t size) {
  PngEncoderState* state = static_cast<PngEncoderState*>(png_get_io_ptr(png));
  DCHECK(state->out);

  size_t old_size = state->out->size();
  state->out->resize(old_size + size);
  memcpy(&(*state->out)[old_size], data, size);
}

void FakeFlushCallback(png_structp png) {
  // We don't need to perform any flushing since we aren't doing real IO, but
  // we're required to provide this function by libpng.
}

void ConvertBGRAtoRGB(const unsigned char* bgra, int pixel_width,
                      unsigned char* rgb, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &bgra[x * 4];
    unsigned char* pixel_out = &rgb[x * 3];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
  }
}

// The type of functions usable for converting between pixel formats.
typedef void (*FormatConverter)(const unsigned char* in, int w,
                                unsigned char* out, bool* is_opaque);

// libpng uses a wacky setjmp-based API, which makes the compiler nervous.
// We constrain all of the calls we make to libpng where the setjmp() is in
// place to this function.
// Returns true on success.
bool DoLibpngWrite(png_struct* png_ptr, png_info* info_ptr,
                   PngEncoderState* state,
                   int width, int height, int row_byte_width,
                   const unsigned char* input,
                   int png_output_color_type, int output_color_components,
                   FormatConverter converter) {
  // Make sure to not declare any locals here -- locals in the presence
  // of setjmp() in C++ code makes gcc complain.

  if (setjmp(png_jmpbuf(png_ptr)))
    return false;

  // Set our callback for libpng to give us the data.
  png_set_write_fn(png_ptr, state, EncoderWriteCallback, FakeFlushCallback);

  png_set_IHDR(png_ptr, info_ptr, width, height, 8, png_output_color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png_ptr, info_ptr);

  if (!converter) {
    // No conversion needed, give the data directly to libpng.
    for (int y = 0; y < height; y ++) {
      png_write_row(png_ptr,
                    const_cast<unsigned char*>(&input[y * row_byte_width]));
    }
  } else {
    // Needs conversion using a separate buffer.
    unsigned char* row = new unsigned char[width * output_color_components];
    for (int y = 0; y < height; y ++) {
      converter(&input[y * row_byte_width], width, row, NULL);
      png_write_row(png_ptr, row);
    }
    delete[] row;
  }

  png_write_end(png_ptr, info_ptr);
  return true;
}

}  // namespace

// static
bool PNGCodec::Encode(const unsigned char* input, ColorFormat format,
                      int w, int h, int row_byte_width,
                      bool discard_transparency,
                      std::vector<unsigned char>* output) {
  // Run to convert an input row into the output row format, NULL means no
  // conversion is necessary.
  FormatConverter converter = NULL;

  int input_color_components, output_color_components;
  int png_output_color_type;
  switch (format) {
    case FORMAT_RGB:
      input_color_components = 3;
      output_color_components = 3;
      png_output_color_type = PNG_COLOR_TYPE_RGB;
      discard_transparency = false;
      break;

    case FORMAT_RGBA:
      input_color_components = 4;
      if (discard_transparency) {
        output_color_components = 3;
        png_output_color_type = PNG_COLOR_TYPE_RGB;
        converter = ConvertRGBAtoRGB;
      } else {
        output_color_components = 4;
        png_output_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        converter = NULL;
      }
      break;

    case FORMAT_BGRA:
      input_color_components = 4;
      if (discard_transparency) {
        output_color_components = 3;
        png_output_color_type = PNG_COLOR_TYPE_RGB;
        converter = ConvertBGRAtoRGB;
      } else {
        output_color_components = 4;
        png_output_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        converter = ConvertBetweenBGRAandRGBA;
      }
      break;

    case FORMAT_SkBitmap:
      input_color_components = 4;
      if (discard_transparency) {
        output_color_components = 3;
        png_output_color_type = PNG_COLOR_TYPE_RGB;
        converter = ConvertSkiatoRGB;
      } else {
        output_color_components = 4;
        png_output_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        converter = ConvertSkiatoRGBA;
      }
      break;

    default:
      NOTREACHED() << "Unknown pixel format";
      return false;
  }

  // Row stride should be at least as long as the length of the data.
  DCHECK(input_color_components * w <= row_byte_width);

  png_struct* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                NULL, NULL, NULL);
  if (!png_ptr)
    return false;
  png_info* info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, NULL);
    return false;
  }

  PngEncoderState state(output);
  bool success = DoLibpngWrite(png_ptr, info_ptr, &state,
                               w, h, row_byte_width, input,
                               png_output_color_type, output_color_components,
                               converter);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  return success;
}

// static
bool PNGCodec::EncodeBGRASkBitmap(const SkBitmap& input,
                                  bool discard_transparency,
                                  std::vector<unsigned char>* output) {
  static const int bbp = 4;

  SkAutoLockPixels lock_input(input);
  DCHECK(input.empty() || input.bytesPerPixel() == bbp);

  return Encode(reinterpret_cast<unsigned char*>(input.getAddr32(0, 0)),
                FORMAT_SkBitmap, input.width(), input.height(),
                input.width() * bbp, discard_transparency, output);
}

}  // namespace gfx
