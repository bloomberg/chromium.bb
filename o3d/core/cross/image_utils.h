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

// This file contains the declaration of functions to help with images.

#ifndef O3D_CORE_CROSS_IMAGE_UTILS_H_
#define O3D_CORE_CROSS_IMAGE_UTILS_H_

#include "base/cross/bits.h"
#include "core/cross/types.h"
#include "core/cross/texture_base.h"

namespace o3d {
namespace image {

// We will fail to load images that are bigger than 4kx4k to avoid security
// risks. GPUs don't usually support bigger sizes anyway.
// The biggest bitmap buffer size with these dimensions is:
// 4k x 4k x 4xsizeof(float) x6 x4/3 (x6 for cube maps, x4/3 for mipmaps)
// That makes 2GB, representable in an unsigned int, so we will avoid wraps.
const unsigned int kMaxImageDimension = 4096u;

enum ImageFileType {
  UNKNOWN,
  TGA,
  JPEG,
  PNG,
  DDS,
};

unsigned int GetNumComponentsForFormat(Texture::Format format);

inline bool IsPOT(unsigned width, unsigned height) {
  return ((width & (width - 1)) == 0) && ((height & (height - 1)) == 0);
}

inline bool CheckImageDimensions(unsigned int width, unsigned int height) {
  return width <= kMaxImageDimension && height <= kMaxImageDimension;
}

// Returns whether or not we can make mips.
bool CanMakeMips(Texture::Format format);

// Gets the number of mip-maps required for a full chain starting at
// width x height.
inline unsigned int ComputeMipMapCount(
    unsigned int width, unsigned int height) {
  return 1 + base::bits::Log2Floor(std::max(width, height));
}

// Gets the smallest power-of-two value that is at least as high as
// dimension. This is the POT dimension used in ScaleUpToPOT.
inline unsigned int ComputePOTSize(unsigned int dimension) {
  return 1 << base::bits::Log2Ceiling(dimension);
}

// Computes one dimension of a mip.
inline unsigned ComputeMipDimension(int level, unsigned dimension) {
  unsigned v = dimension >> level;
  return v > 0 ? v : 1u;
}

// Computes the size of the buffer containing a mip-map chain, given its base
// width, height, format and number of mip-map levels.
size_t ComputeMipChainSize(unsigned int base_width,
                           unsigned int base_height,
                           Texture::Format format,
                           unsigned int num_mipmaps);

inline int ComputePitch(Texture::Format format, unsigned width) {
  if (Texture::IsCompressedFormat(format)) {
    unsigned blocks_across = (width + 3u) / 4u;
    unsigned bytes_per_block = format == Texture::DXT1 ? 8u : 16u;
    return bytes_per_block * blocks_across;
  } else {
    return static_cast<int>(ComputeMipChainSize(width, 1u, format, 1u));
  }
}

// Computes the pitch for a bitmap.
// NOTE: For textures you must get the pitch from the OS.
inline int ComputeMipPitch(Texture::Format format,
                           int level,
                           unsigned width) {
  return ComputePitch(format, ComputeMipDimension(level, width));
}

// Computes the number of bytes of a bitmap pixel buffer.
size_t ComputeBufferSize(unsigned int width,
                         unsigned int height,
                         Texture::Format format);

// Crop part of an image from src, scale it to an arbitrary size
// and paste in dest image. Utility function for all DrawImage
// function in bitmap and textures. Scale operation is based on
// Lanczos resampling.
// Note: this doesn't work for DXTC.
//
// Parameters:
//   format: The format of the images.
//   src: source image which would be copied from.
//   src_pitch: The number of bytes per row in the src image.
//   src_x: x-coordinate of the starting pixel in the src image.
//   src_y: y-coordinate of the starting pixel in the src image.
//   src_width: width of the part in src image to be croped.
//   src_height: height of the part in src image to be croped.
//   dest: dest image which would be copied to.
//   dest_pitch: The number of bytes per row in the dest image.
//   dest_x: x-coordinate of the starting pixel in the dest image.
//   dest_y: y-coordinate of the starting pixel in the dest image.
//   dest_width: width of the part in dest image to be pasted to.
//   dest_height: height of the part in dest image to be pasted to.
//   components: number of components per pixel.
void LanczosScale(Texture::Format format,
                  const void* src, int src_pitch,
                  int src_x, int src_y,
                  int src_width, int src_height,
                  void* dest, int dest_pitch,
                  int dest_x, int dest_y,
                  int dest_width, int dest_height,
                  int components);

// Detects the type of image file based on the filename.
ImageFileType GetFileTypeFromFilename(const char *filename);
//
// Detects the type of image file based on the mime-type.
ImageFileType GetFileTypeFromMimeType(const char *mime_type);

// Adds filler alpha byte (0xff) after every pixel. Assumes buffer was
// allocated with enough storage)
// can convert RGB -> RGBA, BGR -> BGRA, etc.
void XYZToXYZA(uint8 *image_data, int pixel_count);

// Swaps Red and Blue components in the image.
void RGBAToBGRA(uint8 *image_data, int pixel_count);

// Generates a mip-map for 1 level.
// NOTE: this doesn't work for DXTC images.
//
// Parameters:
//   src_width: the width of the source image.
//   src_height: the height of the source image.
//   format: the format of the data.
//   src_data: the data containing the src image.
//   src_pitch: If the format is uncompressed this is the number of bytes
//      per row of pixels. If compressed this value is unused.
//   dst_data: memory for a mip one level smaller then the source.
//   dst_pitch: If the format is uncompressed this is the number of bytes
//      per row of pixels. If compressed this value is unused.
bool GenerateMipmap(unsigned int src_width,
                    unsigned int src_height,
                    Texture::Format format,
                    const void *src_data,
                    int src_pitch,
                    void *dst_data,
                    int dst_pitch);

// Scales an image up to power-of-two textures, using point filtering.
// NOTE: this doesn't work for DXTC images.
//
// Parameters:
//   width: the non-power-of-two width of the original image.
//   height: the non-power-of-two height of the original image.
//   format: the format of the data.
//   src: the data containing the source data of the original image.
//   dst: a buffer with enough space for the power-of-two version. Pixels are
//       written from the end to the beginning so dst can be the same buffer
//       as src.
//   dst_pitch: Number of bytes across 1 row of pixels.
bool ScaleUpToPOT(unsigned int width,
                  unsigned int height,
                  Texture::Format format,
                  const void *src,
                  void *dst,
                  int dst_pitch);

// Scales an image to an arbitrary size, using point filtering.
// NOTE: this doesn't work for DXTC images.
//
// Parameters:
//   src_width: the width of the original image.
//   src_height: the height of the original image.
//   format: the format of the data.
//   src: the data containing the source data of the original image.
//   dst_width: the width of the target image.
//   dst_height: the height of the target image.
//   dst: a buffer with enough space for the target version. Pixels are
//       written from the end to the beginning so dst can be the same buffer
//       as src if the transformation is an upscaling.
//   dst_pitch: Number of bytes across 1 row of pixels.
bool Scale(unsigned int src_width,
           unsigned int src_height,
           Texture::Format format,
           const void *src,
           unsigned int dst_width,
           unsigned int dst_height,
           void *dst,
           int dst_pitch);

// adjust start points and boundaries when using DrawImage data
// in bitmap and textures.
// Parameters:
//   src_x: x-coordinate of the starting pixel in the source image.
//   src_y: y-coordinate of the starting pixel in the source image.
//   src_width: width of the source image to draw.
//   src_height: height of the source image to draw.
//   src_bmp_level: which mip in source.
//   src_bmp_width: original width of source bitmap.
//   src_bmp_height: original height of source bitmap.
//   dest_x: x-coordinate of the starting pixel in the dest image.
//   dest_y: y-coordinate of the starting pixel in the dest image.
//   dest_width: width of the dest image to draw.
//   dest_height: height of the dest image to draw.
//   dest_bmp_level: which mip in dest.
//   dest_bmp_width: original width of dest bitmap.
//   dest_bmp_height: original height of dest bitmap.
// Returns:
//   false if src or dest rectangle is out of boundaries.
bool AdjustDrawImageBoundary(int* src_x, int* src_y,
                             int* src_width, int* src_height,
                             int src_level,
                             int src_bmp_width, int src_bmp_height,
                             int* dest_x, int* dest_y,
                             int* dest_width, int* dest_height,
                             int dest_level,
                             int dest_bmp_width, int dest_bmp_height);

// Checks whether or not we can call SetRect and adjust the inputs
// accordingly so SetRect will work.
//
// Assumes that both the source and destination rectangles are within the
// bounds of their respective images.
//
// Parameters:
//   src_y: A pointer to an int holding the Y position of the source
//       rect. Will be adjusted if SetRect can be called.
//   src_width: The width of the source rect.
//   src_height: The height of the source rect.
//   src_pitch: A pointer to an int holding the pitch of the source. Will be
//       adjusted if SetRect can be called.
//   dst_y: A pointer to an int holding the Y position of the dest rect. Will be
//       adjusted if SetRect can be called.
//   dst_width: The width of the dest rect.
//   dst_height: A pointer to an int holding the height of the dest rect. Will
//       adjusted if SetRect can be called.
// Returns:
//    True if SetRect can be called.
bool AdjustForSetRect(int* src_y,
                      int src_width,
                      int src_height,
                      int* src_pitch,
                      int* dst_y,
                      int dst_width,
                      int* dst_height);

}  // namespace image
}  // namespace o3d

#endif  // O3D_CORE_CROSS_IMAGE_UTILS_H_

