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
#include <vector>
#include "base/cross/bits.h"
#include "core/cross/types.h"
#include "core/cross/texture_base.h"
#include "core/cross/image_utils.h"

class FilePath;

namespace o3d {

class MemoryReadStream;
class RawData;
class Pack;

// Bitmap provides an API for basic image operations on bitmap images,
// including scale and crop. Bitmaps can be created from a RawData object via
// LoadFromRawData(), and also can be transfered to mips of a Texure2D or a
// specific face of TextureCUBE via methods in Texture.

class Bitmap : public ParamObject {
 public:
  typedef SmartPointer<Bitmap> Ref;
  typedef std::vector<Bitmap::Ref> BitmapRefArray;

  explicit Bitmap(ServiceLocator* service_locator);
  virtual ~Bitmap() {}

  enum Semantic {
    FACE_POSITIVE_X,  // NOTE: These must match TextureCUBE::CubeFace
    FACE_NEGATIVE_X,
    FACE_POSITIVE_Y,
    FACE_NEGATIVE_Y,
    FACE_POSITIVE_Z,
    FACE_NEGATIVE_Z,
    IMAGE,            // normal 2d image
    SLICE,            // a slice of a 3d texture.
  };

  // Returns the pitch of the bitmap for a certain level.
  int GetMipPitch(int level) const {
    return image::ComputeMipPitch(format(), level, width());
  }

  // Creates a copy of a bitmap, copying the pixels as well.
  // Parameters:
  //   source: the source bitmap.
  void CopyDeepFrom(const Bitmap &source) {
    Allocate(source.format_, source.width_, source.height_,
             source.num_mipmaps_, source.semantic_);
    memcpy(image_data(), source.image_data(), GetTotalSize());
  }

  // Sets the bitmap parameters from another bitmap, stealing the pixel buffer
  // from the source bitmap.
  // Parameters:
  //   source: the source bitmap.
  void SetFrom(Bitmap *source);

  // Allocates an uninitialized bitmap with specified parameters.
  // Parameters:
  //   format: the format of the pixels.
  //   width: the width of the base image.
  //   height: the height of the base image.
  //   num_mipmaps: the number of mip-maps.
  void Allocate(Texture::Format format,
                unsigned int width,
                unsigned int height,
                unsigned int num_mipmaps,
                Semantic semantic);

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

  // Gets the image data for a given mip-map level.
  // Parameters:
  //   level: mip level to get.
  uint8 *GetMipData(unsigned int level) const;

  // Gets the address of a particular pixel.
  // Parameters:
  //   level: mip level to get.
  uint8 *GetPixelData(unsigned int level, unsigned int x, unsigned int y) const;

  // Gets the size of mip.
  size_t GetMipSize(unsigned int level) const;

  uint8 *image_data() const { return image_data_.get(); }
  Texture::Format format() const { return format_; }
  unsigned int width() const { return width_; }
  unsigned int height() const { return height_; }
  unsigned int num_mipmaps() const { return num_mipmaps_; }
  Semantic semantic() const {
    return semantic_;
  }

  // Returns whether or not the dimensions of the bitmap are power-of-two.
  bool IsPOT() const {
    return image::IsPOT(width_, height_);
  }

  // Loads a bitmap from a file.
  // Parameters:
  //   filename: the name of the file to load.
  //   file_type: the type of file to load. If UNKNOWN, the file type will be
  //              determined from the filename extension, and if it is not a
  //              known extension, all the loaders will be tried.
  //   bitmaps: An array to hold references to the loaded bitmaps.
  static bool LoadFromFile(ServiceLocator* service_locator,
                           const FilePath &filepath,
                           image::ImageFileType file_type,
                           BitmapRefArray* bitmaps);

  // Loads a bitmap from a RawData object.
  // Parameters:
  //   raw_data: contains the bitmap data in one of the known formats
  //   file_type: the format of the bitmap data. If UNKNOWN, the file type
  //              will be determined from the extension from raw_data's uri
  //              and if it is not a known extension, all the loaders will
  //              be tried.
  //   bitmaps: An array to hold references to the loaded bitmaps.
  static bool LoadFromRawData(RawData *raw_data,
                              image::ImageFileType file_type,
                              BitmapRefArray* bitmaps);

  // Flips a bitmap vertically in place.
  // This is needed instead of just using DrawImage because flipping DXT formats
  // using generic algorithms would be lossy and extremely slow to reconvert
  // from a flippable format back to a DXT format.
  void FlipVertically();

  // Returns the contents of the bitmap as a data URL
  // Returns:
  //   A data url that represents the content of the bitmap.
  String ToDataURL();

  // Checks that the alpha channel for the entire bitmap is 1.0
  bool CheckAlphaIsOne() const;

  // Copy pixels from source bitmap. Scales if the width and height of source
  // and dest do not match.
  // Parameters:
  //   source_img: source bitmap which would be drawn.
  //   source_level: level to draw.
  //   source_x: x-coordinate of the starting pixel in the source image.
  //   source_x: y-coordinate of the starting pixel in the source image.
  //   source_width: width of the source image to draw.
  //   source_height: Height of the source image to draw.
  //   dest_level: level to target.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the dest image to draw.
  //   dest_height: height of the dest image to draw.
  void DrawImage(const Bitmap& source_img, int source_level,
                 int source_x, int source_y,
                 int source_width, int source_height,
                 int dest_level,
                 int dest_x, int dest_y,
                 int dest_width, int dest_height);

  // Gets the size of the buffer containing a mip-map chain, given a number of
  // mip-map levels.
  size_t GetMipChainSize(unsigned int num_mipmaps) const {
    return image::ComputeMipChainSize(width(), height(), format(), num_mipmaps);
  }

  // Generates Mips from the source_level for num_levels
  void GenerateMips(int source_level, int num_levels);

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Sets the contents of a Bitmap replacing any previous contents.
  // Parameters:
  //   format: Format of the bitmap.
  //   num_mipmaps: The number of mipmaps.
  //   width: width in pixels.
  //   height: height in pixels.
  //   semantic: the semantic of the bitmap
  //   image_data: The image data. The bitmap will take ownership of this data.
  void SetContents(Texture::Format format,
                   unsigned int num_mipmaps,
                   unsigned int width,
                   unsigned int height,
                   Semantic semantic,
                   scoped_array<uint8>* image_data);

  // Loads bitmaps from a MemoryReadStream.
  // Parameters:
  //   stream: a stream for the bitmap data in one of the known formats
  //   filename: a filename (or uri) of the original bitmap data
  //             (may be an empty string)
  //   file_type: the format of the bitmap data. If UNKNOWN, the file type
  //              will be determined from the extension of |filename|
  //              and if it is not a known extension, all the loaders
  //              will be tried.
  //   bitmaps: An array to hold references to the loaded bitmaps.
  static bool LoadFromStream(ServiceLocator* service_locator,
                             MemoryReadStream *stream,
                             const String &filename,
                             image::ImageFileType file_type,
                             BitmapRefArray* bitmaps);

  static bool LoadFromPNGStream(ServiceLocator* service_locator,
                                MemoryReadStream *stream,
                                const String &filename,
                                BitmapRefArray* bitmaps);

  static bool LoadFromTGAStream(ServiceLocator* service_locator,
                                MemoryReadStream *stream,
                                const String &filename,
                                BitmapRefArray* bitmaps);

  static bool LoadFromDDSStream(ServiceLocator* service_locator,
                                MemoryReadStream *stream,
                                const String &filename,
                                BitmapRefArray* bitmaps);

  static bool LoadFromJPEGStream(ServiceLocator* service_locator,
                                 MemoryReadStream *stream,
                                 const String &filename,
                                 BitmapRefArray* bitmaps);

  bool GenerateMipmaps(unsigned int base_width,
                       unsigned int base_height,
                       Texture::Format format,
                       unsigned int num_mipmaps,
                       uint8 *data);

  // Gets the total size of the bitmap data, counting all faces and mip levels.
  size_t GetTotalSize() const {
    return GetMipChainSize(image::ComputeMipMapCount(width_, height_));
  }

  static size_t ComputeMaxSize(
      unsigned width, unsigned height, Texture::Format format);

  // pointer to the raw bitmap data
  // NOTE: image_data_ is either NULL or it has space for the maximum number
  //     of mips for the current size bitmap, even if they are not used.
  scoped_array<uint8> image_data_;
  // format of the texture this is meant to represent.
  Texture::Format format_;
  // width of the bitmap in pixels.
  int width_;
  // height of the bitmap in pixels.
  int height_;
  // number of valid mipmap levels in this bitmap.
  unsigned int num_mipmaps_;
  // The purpose of the bitmap
  Semantic semantic_;

  O3D_DECL_CLASS(Bitmap, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(Bitmap);
};

typedef Bitmap::BitmapRefArray BitmapRefArray;

class BitmapUncompressed : public Bitmap {
 public:
  explicit BitmapUncompressed(ServiceLocator* service_locator);
};

template <typename T>
class TypedBitmapUncompressed : public BitmapUncompressed {
 public:
  typedef T ComponentType;
  explicit TypedBitmapUncompressed(ServiceLocator* service_locator)
      : BitmapUncompressed(service_locator) {
  }
};

class Bitmap8 : public TypedBitmapUncompressed<uint8> {
 public:
  explicit Bitmap8(ServiceLocator* service_locator);

 private:
  DISALLOW_COPY_AND_ASSIGN(Bitmap8);
};

class Bitmap16F : public TypedBitmapUncompressed<uint16> {
 public:
  explicit Bitmap16F(ServiceLocator* service_locator);

 private:
  DISALLOW_COPY_AND_ASSIGN(Bitmap16F);
};

class Bitmap32F : public TypedBitmapUncompressed<float> {
 public:
  explicit Bitmap32F(ServiceLocator* service_locator);

 private:
  DISALLOW_COPY_AND_ASSIGN(Bitmap32F);
};

class BitmapCompressed : public Bitmap {
 public:
  explicit BitmapCompressed(ServiceLocator* service_locator);

 private:
  DISALLOW_COPY_AND_ASSIGN(BitmapCompressed);
};

class BitmapDXT1 : public BitmapCompressed {
 public:
  explicit BitmapDXT1(ServiceLocator* service_locator);

 private:
  DISALLOW_COPY_AND_ASSIGN(BitmapDXT1);
};

class BitmapDXT3 : public BitmapCompressed {
 public:
  explicit BitmapDXT3(ServiceLocator* service_locator);

 private:
  DISALLOW_COPY_AND_ASSIGN(BitmapDXT3);
};

class BitmapDXT5 : public BitmapCompressed {
 public:
  explicit BitmapDXT5(ServiceLocator* service_locator);

 private:
  DISALLOW_COPY_AND_ASSIGN(BitmapDXT5);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_BITMAP_H_
