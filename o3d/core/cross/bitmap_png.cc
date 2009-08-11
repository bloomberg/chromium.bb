/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
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


// This file contains the image codec operations for PNG files.

// precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include <fstream>
#include "core/cross/bitmap.h"
#include "core/cross/error.h"
#include "core/cross/types.h"
#include "utils/cross/file_path_utils.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "png.h"
#include "utils/cross/dataurl.h"

using file_util::OpenFile;
using file_util::CloseFile;

namespace o3d {

namespace {

// Helper function for LoadFromPNGFile that converts a stream into the
// necessary abstract byte reading function.
void StreamReadData(png_structp png_ptr, png_bytep data, png_size_t length) {
  MemoryReadStream *stream =
    static_cast<MemoryReadStream*>(png_get_io_ptr(png_ptr));
  stream->Read(data, length);
}

// Helper function for ToDataURL that converts a stream into the necessary
// abstract byte writing function.
void StreamWriteData(png_structp png_ptr, png_bytep data, png_size_t length) {
  std::vector<uint8>* stream =
     static_cast<std::vector<uint8>*>(png_get_io_ptr(png_ptr));
  stream->insert(stream->end(),
                 static_cast<uint8*>(data),
                 static_cast<uint8*>(data) + length);
}

// Because libpng requires a flush function according to the docs.
void StreamFlush(png_structp png_ptr) {
}

}  // anonymous namespace

// Loads the raw RGB data from a compressed PNG file.
bool Bitmap::LoadFromPNGStream(ServiceLocator* service_locator,
                               MemoryReadStream *stream,
                               const String &filename,
                               BitmapRefArray* bitmaps) {
  DCHECK(bitmaps);
  // Read the magic header.
  char magic[4];
  size_t bytes_read = stream->Read(magic, sizeof(magic));
  if (bytes_read != sizeof(magic)) {
    DLOG(ERROR) << "PNG file magic header not loaded \"" << filename << "\"";
    return false;
  }

  // Match the magic header to check that this is a PNG file.
  if (png_sig_cmp(reinterpret_cast<png_bytep>(magic), 0, sizeof(magic)) != 0) {
    DLOG(ERROR) << "File is not a PNG file \"" << filename << "\"";
    return false;
  }

  // Load the rest of the PNG file ----------------
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;

  // create the PNG structure (not providing user error functions).
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                   NULL,   // user_error_ptr
                                   NULL,   // user_error_fn
                                   NULL);  // user_warning_fn
  if (png_ptr == NULL)
    return 0;

  // Allocate memory for image information
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
    DLOG(ERROR) << "Cannot allocate working memory for PNG load.";
    return false;
  }

  // NOTE: The following smart pointer needs to be declared before the
  // setjmp so that it is properly destroyed if we jump back.
  scoped_array<uint8> image_data;
  png_bytepp row_pointers = NULL;

  // Set error handling if you are using the setjmp/longjmp method. If any
  // error happens in the following code, we will return here to handle the
  // error.
  if (setjmp(png_jmpbuf(png_ptr))) {
    // If we reach here, a fatal error occurred so free memory and exit.
    DLOG(ERROR) << "Fatal error reading PNG file \"" << filename << "\"";
    if (row_pointers)
      png_free(png_ptr, row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
    return false;
  }

  // Set up our STL stream input control
  png_set_read_fn(png_ptr, stream, &StreamReadData);

  // We have already read some of the signature, advance the pointer.
  png_set_sig_bytes(png_ptr, sizeof(magic));

  // Read the PNG header information.
  png_uint_32 png_width = 0;
  png_uint_32 png_height = 0;
  int png_color_type = 0;
  int png_interlace_type = 0;
  int png_bits_per_channel = 0;
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr,
               info_ptr,
               &png_width,
               &png_height,
               &png_bits_per_channel,
               &png_color_type,
               &png_interlace_type,
               NULL,
               NULL);

  if (!image::CheckImageDimensions(png_width, png_height)) {
    DLOG(ERROR) << "Failed to load " << filename
                << ": dimensions are too large (" << png_width
                << ", " << png_height << ").";
    // Use the png error system to clean up and exit.
    png_error(png_ptr, "PNG image too large");
  }

  // Extract multiple pixels with bit depths of 1, 2, and 4 from a single
  // byte into separate bytes (useful for paletted and grayscale images)
  //
  // png_set_packing(png_ptr);

  // Change the order of packed pixels to least significant bit first
  // (not useful if you are using png_set_packing)
  //
  // png_set_packswap(png_ptr);

  // Number of components in destination data (going to image_data).
  unsigned int dst_components = 0;
  Texture::Format format = Texture::UNKNOWN_FORMAT;
  // Palette vs non-palette.
  if (png_color_type == PNG_COLOR_TYPE_PALETTE) {
    // Expand paletted colors into RGB{A} triplets
    png_set_palette_to_rgb(png_ptr);
  // Gray vs RGB.
  } else if ((png_color_type & PNG_COLOR_MASK_COLOR) == PNG_COLOR_TYPE_RGB) {
    if (png_bits_per_channel != 8) {
      png_error(png_ptr, "PNG image type not recognized");
    }
  } else {
    if (png_bits_per_channel <= 1 ||
        png_bits_per_channel >= 8 ) {
      png_error(png_ptr, "PNG image type not recognized");
    }
    // Expand grayscale images to the full 8 bits from 2, or 4 bits/pixel
    // TODO(o3d): Do we want to expose L/A/LA texture formats ?
    png_set_gray_1_2_4_to_8(png_ptr);
    png_set_gray_to_rgb(png_ptr);
  }
  // Expand paletted or RGB images with transparency to full alpha channels
  // so the data will be available as RGBA quartets.
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png_ptr);
    png_color_type |= PNG_COLOR_MASK_ALPHA;
  }
  // 24-bit RGB image or 32-bit RGBA image.
  if (png_color_type & PNG_COLOR_MASK_ALPHA) {
    format = Texture::ARGB8;
  } else {
    format = Texture::XRGB8;
    // Add alpha byte after each RGB triplet
    png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
  }
  png_set_bgr(png_ptr);
  dst_components = 4;

  // Turn on interlace handling.  REQURIED if you are not using
  // png_read_image().  To see how to handle interlacing passes,
  // see the png_read_row() method below:
  png_set_interlace_handling(png_ptr);

  // Execute any setup steps for each Transform, i.e. to gamma correct and
  // add the background to the palette and update info structure.  REQUIRED
  // if you are expecting libpng to update the palette for you (ie you
  // selected such a transform above).
  png_read_update_info(png_ptr, info_ptr);

  // Allocate storage for the pixels.
  size_t png_image_size =
      image::ComputeMipChainSize(png_width, png_height, format, 1);
  image_data.reset(new uint8[png_image_size]);
  if (image_data.get() == NULL) {
    DLOG(ERROR) << "PNG image memory allocation error \"" << filename << "\"";
    png_error(png_ptr, "Cannot allocate memory for bitmap");
  }

  // Create an array of pointers to the beginning of each row. For some
  // hideous reason the PNG library requires this. At least we don't malloc
  // memory for each row as the examples do.
  row_pointers = static_cast<png_bytep *>(
      png_malloc(png_ptr, png_height * sizeof(png_bytep)));  // NOLINT
  if (row_pointers == NULL) {
    DLOG(ERROR) << "PNG row memory allocation error \"" << filename << "\"";
    png_error(png_ptr, "Cannot allocate memory for row pointers");
  }

  // Fill the row pointer array.
  DCHECK_LE(png_get_rowbytes(png_ptr, info_ptr), png_width * dst_components);
  png_bytep row_ptr = reinterpret_cast<png_bytep>(image_data.get());
  for (unsigned int i = 0; i < png_height; ++i) {
    row_pointers[i] = row_ptr;
    row_ptr += png_width * dst_components;
  }

  // Read the image, applying format transforms and de-interlacing as we go.
  png_read_image(png_ptr, row_pointers);

  // Do not reading any additional chunks using png_read_end()

  // Clean up after the read, and free any memory allocated - REQUIRED
  png_free(png_ptr, row_pointers);
  png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);

  // Success.
  Bitmap::Ref bitmap(new Bitmap(service_locator));
  bitmap->SetContents(format, 1, png_width, png_height, IMAGE, &image_data);
  bitmaps->push_back(bitmap);
  return true;
}

namespace {

bool CreatePNGInUInt8Vector(const Bitmap& bitmap, std::vector<uint8>* buffer) {
  DCHECK(bitmap.format() == Texture::ARGB8);
  DCHECK(bitmap.num_mipmaps() == 1);

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                                NULL, NULL);
  if (!png_ptr) {
    DLOG(ERROR) << "Could not create PNG structure.";
    return false;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    DLOG(ERROR) << "Could not create PNG info structure.";
    png_destroy_write_struct(&png_ptr,  png_infopp_NULL);
    return false;
  }

  unsigned width = bitmap.width();
  unsigned height = bitmap.height();
  scoped_array<png_bytep> row_pointers(new png_bytep[height]);
  for (unsigned int i = 0; i < height; ++i) {
    row_pointers[height - 1 - i] = bitmap.GetMipData(0) + i * width * 4;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    // If we get here, we had a problem reading the file.
    DLOG(ERROR) << "Error while getting dataURL.";
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return false;
  }

  // Set up our STL stream output.
  png_set_write_fn(png_ptr, buffer, &StreamWriteData, &StreamFlush);

  png_set_IHDR(png_ptr, info_ptr, width, height, 8,
               PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_bgr(png_ptr);
  png_set_rows(png_ptr, info_ptr, row_pointers.get());
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, png_voidp_NULL);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  return true;
}

}  // anonymous namespace

String Bitmap::ToDataURL() {
  if (format_ != Texture::ARGB8) {
    O3D_ERROR(service_locator()) << "Can only get data URL from ARGB8 images.";
    return dataurl::kEmptyDataURL;
  }
  if (num_mipmaps_ != 1) {
    O3D_ERROR(service_locator()) <<
        "Can only get data URL from 2d images with no mips.";
    return dataurl::kEmptyDataURL;
  }

  std::vector<uint8> stream;
  if (!CreatePNGInUInt8Vector(*this, &stream)) {
    return dataurl::kEmptyDataURL;
  }

  return dataurl::ToDataURL("image/png", &stream[0], stream.size());
}

}  // namespace o3d
