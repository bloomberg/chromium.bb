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


// This file contains the image file codec operations for OpenGL texture
// loading. Trying to keep this class as independent from the OpenGL API in
// case they need retargeting later on.

// The precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include "core/cross/bitmap.h"
#include <cstring>
#include <cmath>
#include <sys/stat.h>
#include "utils/cross/file_path_utils.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "core/cross/features.h"
#include "core/cross/texture.h"
#include "import/cross/raw_data.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"

using file_util::OpenFile;
using file_util::CloseFile;
using file_util::GetFileSize;

namespace o3d {

O3D_DEFN_CLASS(Bitmap, ParamObject);

Bitmap::Bitmap(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      image_data_(NULL),
      format_(Texture::UNKNOWN_FORMAT),
      width_(0),
      height_(0),
      num_mipmaps_(0),
      is_cubemap_(false) {}

unsigned int Bitmap::GetMipSize(unsigned int level) const {
  unsigned int mip_width = std::max(1U, width() >> level);
  unsigned int mip_height = std::max(1U, height() >> level);
  return image::ComputeMipChainSize(mip_width, mip_height, format(), 1);
}

void Bitmap::Allocate(Texture::Format format,
                      unsigned int width,
                      unsigned int height,
                      unsigned int num_mipmaps,
                      bool cube_map) {
  DCHECK(image::CheckImageDimensions(width, height));
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
    case Texture::ABGR16F:
    case Texture::R32F:
    case Texture::ABGR32F:
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5:
      break;
    default:
      DLOG(FATAL) << "Trying to allocate a bitmap with invalid format";
      break;
  }
  DCHECK(!cube_map || (width == height));
  DCHECK_LE(num_mipmaps, image::ComputeMipMapCount(width, height));
  DCHECK_GT(num_mipmaps, 0u);

  format_ = format;
  width_ = width;
  height_ = height;
  num_mipmaps_ = num_mipmaps;
  is_cubemap_ = cube_map;
  AllocateData();
}

uint8 *Bitmap::GetMipData(unsigned int level) const {
  DCHECK(level < num_mipmaps_);
  DCHECK(!is_cubemap_);
  if (!image_data_.get()) return NULL;
  uint8 *data = image_data_.get();
  return data + GetMipChainSize(level);
}

uint8 *Bitmap::GetFaceMipData(TextureCUBE::CubeFace face,
                              unsigned int level) const {
  DCHECK(level < num_mipmaps_);
  if (!image_data_.get()) return NULL;
  uint8 *data = image_data_.get();
  if (is_cubemap()) {
    data += (face - TextureCUBE::FACE_POSITIVE_X) *
        GetMipChainSize(num_mipmaps_);
  }
  return data + GetMipChainSize(level);
}

void Bitmap::SetRect(
    int level,
    unsigned dst_left,
    unsigned dst_top,
    unsigned src_width,
    unsigned src_height,
    const void* src_data,
    int src_pitch) {
  DCHECK(src_data);
  DCHECK(level < static_cast<int>(num_mipmaps()) && level >= 0);
  unsigned mip_width = image::ComputeMipDimension(level, width());
  unsigned mip_height = image::ComputeMipDimension(level, height());
  DCHECK(dst_left + src_width <= mip_width &&
         dst_top + src_height <= mip_height);
  bool compressed = Texture::IsCompressedFormat(format());
  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  DCHECK(!compressed || entire_rect);

  uint8* dst =
      GetMipData(level) +
      image::ComputeMipChainSize(mip_width, dst_top, format(), 1) +
      image::ComputeMipChainSize(dst_left, 1, format(), 1);

  const uint8* src = static_cast<const uint8*>(src_data);
  if (!compressed) {
    unsigned bytes_per_line = image::ComputePitch(format(), src_width);
    int dst_pitch = image::ComputePitch(format(), mip_width);
    for (unsigned yy = 0; yy < src_height; ++yy) {
      memcpy(dst, src, bytes_per_line);
      src += src_pitch;
      dst += dst_pitch;
    }
  } else {
    memcpy(dst, src,
           image::ComputeMipChainSize(mip_width, mip_height, format(), 1));
  }
}

void Bitmap::SetFaceRect(
    TextureCUBE::CubeFace face,
    int level,
    unsigned dst_left,
    unsigned dst_top,
    unsigned src_width,
    unsigned src_height,
    const void* src_data,
    int src_pitch) {
  DCHECK(src_data);
  DCHECK(level < static_cast<int>(num_mipmaps()) && level >= 0);
  unsigned mip_width = image::ComputeMipDimension(level, width());
  unsigned mip_height = image::ComputeMipDimension(level, height());
  DCHECK(dst_left + src_width <= mip_width &&
         dst_top + src_height <= mip_height);
  bool compressed = Texture::IsCompressedFormat(format());
  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  DCHECK(!compressed || entire_rect);

  uint8* dst =
      GetFaceMipData(face, level) +
      image::ComputeMipChainSize(mip_width, dst_top, format(), 1) +
      image::ComputeMipChainSize(dst_left, 1, format(), 1);

  const uint8* src = static_cast<const uint8*>(src_data);
  if (!compressed) {
    unsigned bytes_per_line = image::ComputePitch(format(), src_width);
    int dst_pitch = image::ComputePitch(format(), mip_width);
    for (unsigned yy = 0; yy < src_height; ++yy) {
      memcpy(dst, src, bytes_per_line);
      src += src_pitch;
      dst += dst_pitch;
    }
  } else {
    memcpy(dst,
           src,
           image::ComputeMipChainSize(mip_width, mip_height, format(), 1));
  }
}


bool Bitmap::LoadFromStream(MemoryReadStream *stream,
                            const String &filename,
                            image::ImageFileType file_type,
                            bool generate_mipmaps) {
  bool success = false;
  // If we don't know what type to load, try to detect it based on the file
  // name.
  if (file_type == image::UNKNOWN) {
    file_type = image::GetFileTypeFromFilename(filename.c_str());
  }

  switch (file_type) {
    case image::TGA:
      success = LoadFromTGAStream(stream, filename, generate_mipmaps);
      break;
    case image::DDS:
      success = LoadFromDDSStream(stream, filename, generate_mipmaps);
      break;
    case image::PNG:
      success = LoadFromPNGStream(stream, filename, generate_mipmaps);
      break;
    case image::JPEG:
      success = LoadFromJPEGStream(stream, filename, generate_mipmaps);
      break;
    case image::UNKNOWN:
    default:
      break;
  }

  if (!success) {
    // At this point we either could not detect the filetype, or possibly
    // the file extension was incorrect (eg. a JPEG image with a .png suffix)
    DLOG(INFO) << "Could not detect file type from filename \""
               << filename << "\". Trying all the loaders.";
    // We will try all the loaders, one by one, starting by the ones that can
    // have an early detect based on magic strings.  We Seek(0) after each try
    // since each attempt changes the stream read position.
    success = LoadFromDDSStream(stream, filename, generate_mipmaps);
    if (!success) {
      stream->Seek(0);
      success = LoadFromPNGStream(stream, filename, generate_mipmaps);
    }

    if (!success) {
      stream->Seek(0);
      success = LoadFromJPEGStream(stream, filename, generate_mipmaps);
    }

    if (!success) {
      stream->Seek(0);
      success = LoadFromTGAStream(stream, filename, generate_mipmaps);
    }
  }

  if (success) {
    Features* features = service_locator()->GetService<Features>();
    DCHECK(features);
    if (features->flip_textures()) {
      FlipVertically();
    }
  } else {
    DLOG(ERROR) << "Failed to load image \"" << filename
                << "\": unknown file type";
  }
  return success;
}

// Given an arbitrary bitmap file, load it all into memory and then call our
// stream loader
bool Bitmap::LoadFromFile(const FilePath &filepath,
                          image::ImageFileType file_type,
                          bool generate_mipmaps) {
  // Open the file.
  bool result = false;
  String filename = FilePathToUTF8(filepath);
  FILE *file = OpenFile(filepath, "rb");

  if (!file) {
    DLOG(ERROR) << "bitmap file not found \"" << filename << "\"";
  } else {
    // Determine the file's length
    int64 file_size64;
    if (!GetFileSize(filepath, &file_size64)) {
      DLOG(ERROR) << "error getting bitmap file size \"" << filename << "\"";
    } else {
      if (file_size64 > 0xffffffffLL) {
        DLOG(ERROR) << "bitmap file is too large \"" << filename << "\"";
      } else {
        size_t file_length = static_cast<size_t>(file_size64);

        // Load the compressed image data into memory
        MemoryBuffer<uint8> file_contents(file_length);
        uint8 *p = file_contents;
        if (fread(p, file_length, 1, file) != 1) {
          DLOG(ERROR) << "error reading bitmap file \"" << filename << "\"";
        } else {
          // And create the bitmap from a memory stream
          MemoryReadStream stream(file_contents, file_length);
          result = LoadFromStream(&stream, filename, file_type,
                                  generate_mipmaps);
        }
      }
    }
    CloseFile(file);
  }

  return result;
}

// Given a RawData object containing image data in one of our known formats,
// decide which image format it is and call the correct loading function.
bool Bitmap::LoadFromRawData(RawData *raw_data,
                             image::ImageFileType file_type,
                             bool generate_mipmaps) {
  String filename = raw_data->uri();

  // GetData() returns NULL if it, for example, cannot open the temporary data
  // file. In that case, it invokes the error callback. We just have to be
  // careful not to dereference it.
  const uint8* data = raw_data->GetData();
  if (!data) {
    return false;
  }

  MemoryReadStream stream(data, raw_data->GetLength());

  return LoadFromStream(&stream, filename, file_type, generate_mipmaps);
}

void Bitmap::DrawImage(const Bitmap& src_img,
                       int src_x, int src_y,
                       int src_width, int src_height,
                       int dst_x, int dst_y,
                       int dst_width, int dst_height) {
  DCHECK(src_img.image_data());
  DCHECK(image_data());

  // Clip source and destination rectangles to
  // source and destination bitmaps.
  // if src or dest rectangle is out of boundary,
  // do nothing and return.
  if (!image::AdjustDrawImageBoundary(&src_x, &src_y,
                                      &src_width, &src_height,
                                      src_img.width_, src_img.height_,
                                      &dst_x, &dst_y,
                                      &dst_width, &dst_height,
                                      width_, height_))
    return;

  unsigned int components = 0;
  // check formats of source and dest images.
  // format of source and dest should be the same.
  if (src_img.format_ != format_) {
    O3D_ERROR(service_locator()) << "DrawImage does not support "
                                 << "different formats.";
    return;
  }
  // if src and dest are in the same size and drawImage is copying
  // the entire bitmap on dest image, just perform memcpy.
  if (src_x == 0 && src_y == 0 && dst_x == 0 && dst_y == 0 &&
      src_img.width_ == width_ && src_img.height_ == height_ &&
      src_width == src_img.width_ && src_height == src_img.height_ &&
      dst_width == width_ && dst_height == height_) {
    memcpy(image_data(), src_img.image_data(), GetTotalSize());
    return;
  }

  // if drawImage is not copying the whole bitmap, we need to check
  // the format. currently only support XRGB8 and ARGB8
  if (src_img.format_ == Texture::XRGB8 ||
      src_img.format_ == Texture::ARGB8) {
    components = 4;
  } else {
    O3D_ERROR(service_locator()) << "DrawImage does not support format: "
                                 << src_img.format_ << " unless src and "
                                 << "dest images are in the same size and "
                                 << "copying the entire bitmap";
    return;
  }

  uint8* src_img_data = src_img.image_data();
  uint8* dst_img_data = image_data();

  // crop part of image from src img, scale it in
  // bilinear interpolation fashion, and paste it
  // on dst img.
  image::LanczosScale(src_img_data, src_x, src_y,
                      src_width, src_height,
                      src_img.width_, src_img.height_,
                      dst_img_data, width_ * components,
                      dst_x, dst_y,
                      dst_width, dst_height,
                      width_, height_, components);
}


void Bitmap::GenerateMips(int source_level, int num_levels) {
  if (source_level >= num_mipmaps() || source_level < 0) {
    O3D_ERROR(service_locator()) << "source level out of range.";
    return;
  }
  if (source_level + num_levels >= num_mipmaps() || num_levels < 0) {
    O3D_ERROR(service_locator()) << "num levels out of range.";
    return;
  }

  GenerateMipmaps(image::ComputeMipDimension(source_level, width()),
                  image::ComputeMipDimension(source_level, height()),
                  format(),
                  num_levels,
                  GetMipData(source_level));
}

// NOTE: This only works for Bitmap since Bitmap knows the pitch.
bool Bitmap::GenerateMipmaps(unsigned int base_width,
                             unsigned int base_height,
                             Texture::Format format,
                             unsigned int num_mipmaps,
                             uint8 *data) {
  DCHECK(image::CheckImageDimensions(base_width, base_height));
  unsigned int components = image::GetNumComponentsForFormat(format);
  if (components == 0) {
    DLOG(ERROR) << "Mip-map generation not supported for format: " << format;
    return false;
  }
  DCHECK_GE(std::max(base_width, base_height) >> (num_mipmaps - 1), 1u);
  uint8 *mip_data = data;
  unsigned int mip_width = base_width;
  unsigned int mip_height = base_height;
  for (unsigned int level = 1; level < num_mipmaps; ++level) {
    unsigned int prev_width = mip_width;
    unsigned int prev_height = mip_height;
    uint8 *prev_data = mip_data;
    mip_data += components * mip_width * mip_height;
    DCHECK_EQ(mip_data, data + image::ComputeMipChainSize(
                  base_width, base_height, format, level));
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
    image::GenerateMipmap(
        prev_width, prev_height, format,
        prev_data, image::ComputePitch(format, prev_width),
        mip_data, image::ComputePitch(format, mip_width));
  }

  return true;
}

// Checks that all the alpha values are 1.0
bool Bitmap::CheckAlphaIsOne() const {
  if (!image_data())
    return false;

  switch (format()) {
    case Texture::XRGB8:
      return true;
    case Texture::ARGB8: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const uint8 *data = GetFaceMipData(
              static_cast<TextureCUBE::CubeFace>(face),
              level) + 3;
          const uint8* end = data + image::ComputeBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          while (data < end) {
            if (*data != 255) {
              return false;
            }
            data += 4;
          }
        }
      }
      break;
    }
    case Texture::DXT1: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const uint8 *data = GetFaceMipData(
              static_cast<TextureCUBE::CubeFace>(face),
              level);
          const uint8* end = data + image::ComputeBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          DCHECK((end - data) % 8 == 0);
          while (data < end) {
            int color0 = static_cast<int>(data[0]) |
                         static_cast<int>(data[1]) << 8;
            int color1 = static_cast<int>(data[2]) |
                         static_cast<int>(data[3]) << 8;
            if (color0 < color1) {
              return false;
            }
            data += 8;
          }
        }
      }
      break;
    }
    case Texture::DXT3:
    case Texture::DXT5:
      return false;
    case Texture::ABGR16F: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const uint8 *data = GetFaceMipData(
              static_cast<TextureCUBE::CubeFace>(face),
              level) + 6;
          const uint8* end = data + image::ComputeBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          while (data < end) {
            if (data[0] != 0x00 || data[1] != 0x3C) {
              return false;
            }
            data += 8;
          }
        }
      }
      break;
    }
    case Texture::R32F:
      return true;
    case Texture::ABGR32F: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const uint8* data = GetFaceMipData(
              static_cast<TextureCUBE::CubeFace>(face),
              level) + 12;
          const uint8* end = data + image::ComputeBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          while (data < end) {
            if (*(reinterpret_cast<const float*>(data)) != 1.0f) {
              return false;
            }
            data += 16;
          }
        }
      }
      break;
    }
    case Texture::UNKNOWN_FORMAT:
      return false;
  }
  return true;
}

ObjectBase::Ref Bitmap::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Bitmap(service_locator));
}

}  // namespace o3d
