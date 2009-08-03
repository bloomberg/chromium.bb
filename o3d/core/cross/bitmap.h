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


// This file contains the declaration of Bitmap helper class that can load
// raw 24- and 32-bit bitmaps from popular image formats. The Bitmap class
// also interprets the file format to record the correct OpenGL buffer format.
//
// Trying to keep this class independent from the OpenGL API in case they
// need retargeting later on.

#ifndef O3D_CORE_CROSS_BITMAP_H_
#define O3D_CORE_CROSS_BITMAP_H_

#include <stdlib.h>

#include "base/cross/bits.h"
#include "core/cross/types.h"
#include "core/cross/texture.h"

class FilePath;

namespace o3d {

class MemoryReadStream;
class RawData;
class Pack;

// Bitmap provides an API for basic image operations on bitmap images,
// including scale and crop. The contents of bitmap can be created from
// a RawData object via LoadFromRawData(), and also can be transfered
// to mip of a Texure2D or a specific face of TextureCUBE via methods
// in Texture.

class Bitmap : public ParamObject {
 public:
  typedef SmartPointer<Bitmap> Ref;

  explicit Bitmap(ServiceLocator* service_locator);
  virtual ~Bitmap() {}

  // We will fail to load images that are bigger than 4kx4k to avoid security
  // risks. GPUs don't usually support bigger sizes anyway.
  // The biggest bitmap buffer size with these dimensions is:
  // 4k x 4k x 4xsizeof(float) x6 x4/3 (x6 for cube maps, x4/3 for mipmaps)
  // That makes 2GB, representable in an unsigned int, so we will avoid wraps.
  static const unsigned int kMaxImageDimension = 4096;
  enum ImageFileType {
    UNKNOWN,
    TGA,
    JPEG,
    PNG,
    DDS,
  };

  static bool CheckImageDimensions(unsigned int width, unsigned int height) {
    return width > 0 && height > 0 &&
        width <= kMaxImageDimension && height < kMaxImageDimension;
  }

  // Computes the width and height of a mip.
  static void GetMipSize(int level,
                         unsigned width,
                         unsigned height,
                         unsigned* mip_width,
                         unsigned* mip_height) {
    unsigned w = width >> level;
    unsigned h = height >> level;
    *mip_width = w > 0 ? w : 1u;
    *mip_height = h > 0 ? h : 1u;
  }

  // Creates a copy of a bitmap, copying the pixels as well.
  // Parameters:
  //   source: the source bitmap.
  void CopyDeepFrom(const Bitmap &source) {
    Allocate(source.format_, source.width_, source.height_,
             source.num_mipmaps_, source.is_cubemap_);
    memcpy(image_data(), source.image_data(), GetTotalSize());
  }

  // Sets the bitmap parameters from another bitmap, stealing the pixel buffer
  // from the source bitmap.
  // Parameters:
  //   source: the source bitmap.
  void SetFrom(Bitmap *source) {
    image_data_.reset();
    format_ = source->format_;
    width_ = source->width_;
    height_ = source->height_;
    num_mipmaps_ = source->num_mipmaps_;
    is_cubemap_ = source->is_cubemap_;
    image_data_.swap(source->image_data_);
  }

  // Allocates an uninitialized bitmap with specified parameters.
  // Parameters:
  //   format: the format of the pixels.
  //   width: the width of the base image.
  //   height: the height of the base image.
  //   num_mipmaps: the number of mip-maps.
  //   cube_map: true if creating a cube map.
  void Allocate(Texture::Format format,
                unsigned int width,
                unsigned int height,
                unsigned int num_mipmaps,
                bool cube_map);

  // Allocates a bitmap with initialized parameters.
  // data is zero-initialized
  void AllocateData() {
    image_data_.reset(new uint8[GetTotalSize()]);
    memset(image_data_.get(), 0, GetTotalSize());
  }

  // Frees the data owned by the bitmap.
  void FreeData() {
    image_data_.reset(NULL);
  }

  // Sets a rectangular region of this bitmap.
  // If the bitmap is a DXT format, the only acceptable values
  // for left, top, width and height are 0, 0, bitmap->width, bitmap->height
  //
  // Parameters:
  //   level: The mipmap level to modify
  //   dst_left: The left edge of the rectangular area to modify.
  //   dst_top: The top edge of the rectangular area to modify.
  //   width: The width of the rectangular area to modify.
  //   height: The of the rectangular area to modify.
  //   src_data: The source pixels.
  //   src_pitch: If the format is uncompressed this is the number of bytes
  //      per row of pixels. If compressed this value is unused.
  void SetRect(int level,
               unsigned dst_left,
               unsigned dst_top,
               unsigned width,
               unsigned height,
               const void* src_data,
               int src_pitch);

  // Sets a rectangular region of this bitmap.
  // If the bitmap is a DXT format, the only acceptable values
  // for left, top, width and height are 0, 0, bitmap->width, bitmap->height
  //
  // Parameters:
  //   level: The mipmap level to modify
  //   dst_left: The left edge of the rectangular area to modify.
  //   dst_top: The top edge of the rectangular area to modify.
  //   width: The width of the rectangular area to modify.
  //   height: The of the rectangular area to modify.
  //   src_data: The source pixels.
  //   src_pitch: If the format is uncompressed this is the number of bytes
  //      per row of pixels. If compressed this value is unused.
  void SetFaceRect(TextureCUBE::CubeFace face,
                   int level,
                   unsigned dst_left,
                   unsigned dst_top,
                   unsigned width,
                   unsigned height,
                   const void* src_data,
                   int src_pitch);

  // Gets the total size of the bitmap data, counting all faces and mip levels.
  unsigned int GetTotalSize() {
    return (is_cubemap_ ? 6 : 1) *
        GetMipChainSize(width_, height_, format_, num_mipmaps_);
  }

  // Computes the number of bytes of a texture pixel buffer.
  static unsigned int GetBufferSize(unsigned int width,
                                    unsigned int height,
                                    Texture::Format format);

  // Gets the image data for a given mip-map level.
  // Parameters:
  //   level: mip level to get.
  uint8 *GetMipData(unsigned int level) const;

  // Gets the image data for a given mip-map level and cube map face.
  // Parameters:
  //   face: face of cube to get.
  //   level: mip level to get.
  uint8 *GetFaceMipData(TextureCUBE::CubeFace face,
                        unsigned int level) const;

  uint8 *image_data() const { return image_data_.get(); }
  Texture::Format format() const { return format_; }
  unsigned int width() const { return width_; }
  unsigned int height() const { return height_; }
  unsigned int num_mipmaps() const { return num_mipmaps_; }
  bool is_cubemap() const { return is_cubemap_; }

  // Returns whether or not the dimensions of the bitmap are power-of-two.
  bool IsPOT() const {
    return ((width_ & (width_ - 1)) == 0) && ((height_ & (height_ - 1)) == 0);
  }

  void set_format(Texture::Format format) { format_ = format; }
  void set_width(unsigned int n) { width_ = n; }
  void set_height(unsigned int n) { height_ = n; }
  void set_num_mipmaps(unsigned int n) { num_mipmaps_ = n; }
  void set_is_cubemap(bool is_cubemap) { is_cubemap_ = is_cubemap; }

  // Loads a bitmap from a file.
  // Parameters:
  //   filename: the name of the file to load.
  //   file_type: the type of file to load. If UNKNOWN, the file type will be
  //              determined from the filename extension, and if it is not a
  //              known extension, all the loaders will be tried.
  //   generate_mipmaps: whether or not to generate all the mip-map levels.
  bool LoadFromFile(const FilePath &filepath,
                    ImageFileType file_type,
                    bool generate_mipmaps);

  // Loads a bitmap from a RawData object.
  // Parameters:
  //   raw_data: contains the bitmap data in one of the known formats
  //   file_type: the format of the bitmap data. If UNKNOWN, the file type
  //              will be determined from the extension from raw_data's uri
  //              and if it is not a known extension, all the loaders will
  //              be tried.
  //   generate_mipmaps: whether or not to generate all the mip-map levels.
  bool LoadFromRawData(RawData *raw_data,
                       ImageFileType file_type,
                       bool generate_mipmaps);

  // Loads a bitmap from a MemoryReadStream.
  // Parameters:
  //   stream: a stream for the bitmap data in one of the known formats
  //   filename: a filename (or uri) of the original bitmap data
  //             (may be an empty string)
  //   file_type: the format of the bitmap data. If UNKNOWN, the file type
  //              will be determined from the extension of |filename|
  //              and if it is not a known extension, all the loaders
  //              will be tried.
  //   generate_mipmaps: whether or not to generate all the mip-map levels.
  bool LoadFromStream(MemoryReadStream *stream,
                      const String &filename,
                      ImageFileType file_type,
                      bool generate_mipmaps);

  bool LoadFromPNGStream(MemoryReadStream *stream,
                         const String &filename,
                         bool generate_mipmaps);

  bool LoadFromTGAStream(MemoryReadStream *stream,
                         const String &filename,
                         bool generate_mipmaps);

  bool LoadFromDDSStream(MemoryReadStream *stream,
                         const String &filename,
                         bool generate_mipmaps);

  bool LoadFromJPEGStream(MemoryReadStream *stream,
                          const String &filename,
                          bool generate_mipmaps);

  // Saves to a PNG file. The image must be of the ARGB8 format, be a 2D image
  // with no mip-maps (only the base level).
  // Parameters:
  //   filename: the name of the file to into.
  // Returns:
  //   true if successful.
  bool SaveToPNGFile(const char* filename);

  // Checks that the alpha channel for the entire bitmap is 1.0
  bool CheckAlphaIsOne() const;

  // Copy pixels from source bitmap. Scales if the width and height of source
  // and dest do not match.
  // Parameters:
  //   source_img: source bitmap which would be drawn.
  //   source_x: x-coordinate of the starting pixel in the source image.
  //   source_x: y-coordinate of the starting pixel in the source image.
  //   source_width: width of the source image to draw.
  //   source_height: Height of the source image to draw.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the dest image to draw.
  //   dest_height: height of the dest image to draw.
  void DrawImage(Bitmap* source_img, int source_x, int source_y,
                 int source_width, int source_height,
                 int dest_x, int dest_y,
                 int dest_width, int dest_height);

  // Crop part of an image from src, scale it to an arbitrary size
  // and paste in dest image. Utility function for all DrawImage
  // function in bitmap and textures. Scale operation is based on
  // Lanczos resampling.
  // Note: this doesn't work for DXTC, or floating-point images.
  //
  // Parameters:
  //   src: source image which would be copied from.
  //   src_x: x-coordinate of the starting pixel in the src image.
  //   src_y: y-coordinate of the starting pixel in the src image.
  //   src_width: width of the part in src image to be croped.
  //   src_height: height of the part in src image to be croped.
  //   src_img_width: width of the src image.
  //   src_img_height: height of the src image.
  //   dest: dest image which would be copied to.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the part in dest image to be pasted to.
  //   dest_height: height of the part in dest image to be pasted to.
  //   dest_img_width: width of the dest image.
  //   dest_img_height: height of the src image.
  //   component: size of each pixel in terms of array element.
  // Returns:
  //   true if crop and scale succeeds.
  static void LanczosScale(const uint8* src,
                           int src_x, int src_y,
                           int src_width, int src_height,
                           int src_img_width, int src_img_height,
                           uint8* dest, int dest_pitch,
                           int dest_x, int dest_y,
                           int dest_width, int dest_height,
                           int dest_img_width, int dest_img_height,
                           int component);

  // Detects the type of image file based on the filename.
  static ImageFileType GetFileTypeFromFilename(const char *filename);
  // Detects the type of image file based on the mime-type.
  static ImageFileType GetFileTypeFromMimeType(const char *mime_type);

  // Adds filler alpha byte (0xff) after every pixel. Assumes buffer was
  // allocated with enough storage)
  // can convert RGB -> RGBA, BGR -> BGRA, etc.
  static void XYZToXYZA(uint8 *image_data, int pixel_count);

  // Swaps Red and Blue components in the image.
  static void RGBAToBGRA(uint8 *image_data, int pixel_count);

  // Gets the number of mip-maps required for a full chain starting at
  // width x height.
  static unsigned int GetMipMapCount(unsigned int width, unsigned int height) {
    return 1 + base::bits::Log2Floor(std::max(width, height));
  }

  // Gets the smallest power-of-two value that is at least as high as
  // dimension. This is the POT dimension used in ScaleUpToPOT.
  static unsigned int GetPOTSize(unsigned int dimension) {
    return 1 << base::bits::Log2Ceiling(dimension);
  }

  // Gets the size of the buffer containing a mip-map chain, given its base
  // width, height, format and number of mip-map levels.
  static unsigned int GetMipChainSize(unsigned int base_width,
                                      unsigned int base_height,
                                      Texture::Format format,
                                      unsigned int num_mipmaps);

  // Generates mip-map levels for a single image, using the data from the base
  // level.
  // NOTE: this doesn't work for DXTC, or floating-point images.
  //
  // Parameters:
  //   base_width: the width of the base image.
  //   base_height: the height of the base image.
  //   format: the format of the data.
  //   num_mipmaps: the number of mipmaps to generate.
  //   data: the data containing the base image, and enough space for the
  //   mip-maps.
  static bool GenerateMipmaps(unsigned int base_width,
                              unsigned int base_height,
                              Texture::Format format,
                              unsigned int num_mipmaps,
                              uint8 *data);

  // Scales an image up to power-of-two textures, using point filtering.
  // NOTE: this doesn't work for DXTC, or floating-point images.
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
  static bool ScaleUpToPOT(unsigned int width,
                           unsigned int height,
                           Texture::Format format,
                           const uint8 *src,
                           uint8 *dst,
                           int dst_pitch);

  // Scales an image to an arbitrary size, using point filtering.
  // NOTE: this doesn't work for DXTC, or floating-point images.
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
  static bool Scale(unsigned int src_width,
                    unsigned int src_height,
                    Texture::Format format,
                    const uint8 *src,
                    unsigned int dst_width,
                    unsigned int dst_height,
                    uint8 *dst,
                    int dst_pitch);

  // adjust start points and boundaries when using DrawImage data
  // in bitmap and textures.
  // Parameters:
  //   src_x: x-coordinate of the starting pixel in the source image.
  //   src_y: y-coordinate of the starting pixel in the source image.
  //   src_width: width of the source image to draw.
  //   src_height: height of the source image to draw.
  //   src_bmp_width: original width of source bitmap.
  //   src_bmp_height: original height of source bitmap.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the dest image to draw.
  //   dest_height: height of the dest image to draw.
  //   dest_bmp_width: original width of dest bitmap.
  //   dest_bmp_height: original height of dest bitmap.
  // Returns:
  //   false if src or dest rectangle is out of boundaries.
  static bool AdjustDrawImageBoundary(int* src_x, int* src_y,
                                      int* src_width, int* src_height,
                                      int src_bmp_width, int src_bmp_height,
                                      int* dest_x, int* dest_y,
                                      int* dest_width, int* dest_height,
                                      int dest_bmp_width, int dest_bmp_height);

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // pointer to the raw bitmap data
  scoped_array<uint8> image_data_;
  // format of the texture this is meant to represent.
  Texture::Format format_;
  // width of the bitmap in pixels.
  int width_;
  // height of the bitmap in pixels.
  int height_;
  // number of mipmap levels in this texture.
  unsigned int num_mipmaps_;
  // is this cube-map data
  bool is_cubemap_;

  // utility function used in AdjustDrawImageBoundary.
  // It adjusts start point and related measures
  // for a specific dimension.
  // Parameter:
  //   src_a: the coordinate which is negative.
  //   dest_a: same coordinate in the other image.
  //   src_length: length measure of source image to draw.
  //   dest_length: length measure of dest image to draw.
  //   src_bmp_length: length measure of src image.
  // Returns:
  //   true if adjust is successful.
  static bool AdjustDrawImageBoundHelper(int* src_a, int* dest_a,
                                         int* src_length, int* dest_length,
                                         int src_bmp_length);

  // utility function for LanczosScale function.
  // Given an image, It can scale the image in one dimension to the new
  // length using a Lanczos3 windowsed-sinc filter. This filter has
  // fairly large support, in choosing it we favor quality over speed.
  // In parameters we assume the current dimension we are scaling is
  // width, but by changing the parameter, we can easily using this
  // function on height as well.
  // Parameter:
  //   src: source image which would be copied from.
  //   src_x: x-coordinate of the starting pixel in the source image.
  //   src_y: y-coordinate of the starting pixel in the source image.
  //   width: width of the part in src image to be croped.
  //   height: height of the part in src image to be croped.
  //   src_bmp_width: original width of source bitmap.
  //   src_bmp_height: original height of source bitmap.
  //   dest: dest image which would be copied to.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_bmp_width: original width of dest bitmap.
  //   dest_bmp_height: original height of dest bitmap.
  //   isWidth: which dimension we are working on.
  //   components: size of each pixel in terms of array element.
  static void LanczosResize1D(const uint8* src, int src_x, int src_y,
                              int width, int height,
                              int src_bmp_width, int src_bmp_height,
                              uint8* dest, int dest_pitch,
                              int dest_x, int dest_y,
                              int nwidth,
                              int dest_bmp_width, int dest_bmp_height,
                              bool isWidth, int components);

  O3D_DECL_CLASS(Bitmap, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(Bitmap);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_BITMAP_H_
