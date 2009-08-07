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


// This file contains the definition of the Texture2D and TextureCUBE classes.

#include "core/cross/precompile.h"
#include <cmath>
#include "core/cross/texture.h"
#include "core/cross/bitmap.h"
#include "core/cross/renderer.h"
#include "core/cross/client_info.h"
#include "core/cross/error.h"

namespace o3d {

O3D_DEFN_CLASS(Texture2D, Texture);
O3D_DEFN_CLASS(TextureCUBE, Texture);

const char* Texture2D::kWidthParamName =
    O3D_STRING_CONSTANT("width");
const char* Texture2D::kHeightParamName =
    O3D_STRING_CONSTANT("height");
const char* TextureCUBE::kEdgeLengthParamName =
    O3D_STRING_CONSTANT("edgeLength");

Texture2D::Texture2D(ServiceLocator* service_locator,
                     int width,
                     int height,
                     Format format,
                     int levels,
                     bool alpha_is_one,
                     bool resize_to_pot,
                     bool enable_render_surfaces)
    : Texture(service_locator, format, levels, alpha_is_one,
              resize_to_pot, enable_render_surfaces),
      locked_levels_(0) {
  RegisterReadOnlyParamRef(kWidthParamName, &width_param_);
  RegisterReadOnlyParamRef(kHeightParamName, &height_param_);
  width_param_->set_read_only_value(width);
  height_param_->set_read_only_value(height);

  ClientInfoManager* client_info_manager =
      service_locator->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    static_cast<int>(image::ComputeMipChainSize(
        width, height, format, levels)));
}

Texture2D::~Texture2D() {
  if (locked_levels_ != 0) {
    O3D_ERROR(service_locator())
        << "Texture2D \"" << name()
        << "\" was never unlocked before being destroyed.";
  }

  ClientInfoManager* client_info_manager =
      service_locator()->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    -static_cast<int>(image::ComputeMipChainSize(width(),
                                                 height(),
                                                 format(),
                                                 levels())));
}

void Texture2D::DrawImage(const Bitmap& src_img,
                          int src_x, int src_y,
                          int src_width, int src_height,
                          int dst_x, int dst_y,
                          int dst_width, int dst_height, int dest_mip) {
  DCHECK(src_img.image_data());

  if (dest_mip < 0 || dest_mip >= levels()) {
    O3D_ERROR(service_locator()) << "Mip out of range";
  }

  unsigned int mip_width = std::max(1, width() >> dest_mip);
  unsigned int mip_height = std::max(1, height() >> dest_mip);

  // Clip source and destination rectangles to
  // source and destination bitmaps.
  // if src or dest rectangle is out of boundary,
  // do nothing and return.
  if (!image::AdjustDrawImageBoundary(&src_x, &src_y,
                                      &src_width, &src_height,
                                      src_img.width(), src_img.height(),
                                      &dst_x, &dst_y,
                                      &dst_width, &dst_height,
                                      mip_width, mip_height)) {
    return;
  }

  unsigned int components = 0;
  // check formats of source and dest images.
  // format of source and dest should be the same.
  if (src_img.format() != format()) {
    O3D_ERROR(service_locator()) << "DrawImage does not support "
                                 << "different formats.";
    return;
  }
  // if src and dest are in the same size and drawImage is copying
  // the entire bitmap on dest image, just perform memcpy.
  if (src_x == 0 && src_y == 0 && dst_x == 0 && dst_y == 0 &&
      src_img.width() == mip_width && src_img.height() == mip_height &&
      static_cast<unsigned int>(src_width) == src_img.width() &&
      static_cast<unsigned int>(src_height) == src_img.height() &&
      static_cast<unsigned int>(dst_width) == mip_width &&
      static_cast<unsigned int>(dst_height) == mip_height) {
    SetRect(dest_mip, 0, 0, mip_width, mip_height,
            src_img.image_data(),
            src_img.GetMipPitch(0));
    return;
  }
  if (src_img.format() == Texture::XRGB8 ||
      src_img.format() == Texture::ARGB8) {
    components = 4;
  } else {
    O3D_ERROR(service_locator()) << "DrawImage does not support format: "
                                 << src_img.format() << " unless src and "
                                 << "dest images are in the same size and "
                                 << "copying the entire bitmap";
    return;
  }

  LockHelper helper(this, dest_mip);
  uint8* mip_data = helper.GetDataAs<uint8>();
  if (!mip_data) {
    return;
  }

  uint8* src_img_data = src_img.image_data();

  image::LanczosScale(src_img_data, src_x, src_y,
                      src_width, src_height,
                      src_img.width(), src_img.height(),
                      mip_data, helper.pitch(),
                      dst_x, dst_y,
                      dst_width, dst_height,
                      mip_width, mip_height, components);
}

void Texture2D::SetFromBitmap(const Bitmap& bitmap) {
  DCHECK(bitmap.image_data());
  if (bitmap.width() != static_cast<unsigned>(width()) ||
      bitmap.height() != static_cast<unsigned>(height()) ||
      bitmap.format() != format() || bitmap.is_cubemap()) {
    O3D_ERROR(service_locator())
        << "bitmap must be the same format and dimensions as texture";
    return;
  }

  int last_level = std::min<int>(bitmap.num_mipmaps(), levels());
  for (int level = 0; level < last_level; ++level) {
    SetRect(level, 0, 0,
            image::ComputeMipDimension(level, width()),
            image::ComputeMipDimension(level, height()),
            bitmap.GetMipData(level),
            bitmap.GetMipPitch(level));
  }
}

void Texture2D::GenerateMips(int source_level, int num_levels) {
  if (source_level < 0 || source_level >= levels()) {
    O3D_ERROR(service_locator()) << "source level out of range";
    return;
  }
  if (source_level + num_levels >= levels()) {
    O3D_ERROR(service_locator()) << "num levels out of range";
    return;
  }

  for (int ii = 0; ii < num_levels; ++ii) {
    int level = source_level + ii;
    Texture2D::LockHelper src_helper(this, level);
    Texture2D::LockHelper dst_helper(this, level + 1);
    const uint8* src_data = src_helper.GetDataAs<const uint8>();
    if (!src_data) {
      O3D_ERROR(service_locator())
          << "could not lock source texture.";
      return;
    }
    uint8* dst_data = dst_helper.GetDataAs<uint8>();
    if (!dst_data) {
      O3D_ERROR(service_locator())
          << "could not lock destination texture.";
      return;
    }

    unsigned int src_width = image::ComputeMipDimension(level, width());
    unsigned int src_height = image::ComputeMipDimension(level, height());
    image::GenerateMipmap(src_width, src_height, format(),
                          src_data, src_helper.pitch(),
                          dst_data, dst_helper.pitch());
  }
}

ObjectBase::Ref Texture2D::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref();
}

Texture2D::LockHelper::LockHelper(Texture2D* texture, int level)
    : texture_(texture),
      level_(level),
      data_(NULL),
      locked_(false) {
}

Texture2D::LockHelper::~LockHelper() {
  if (locked_) {
    texture_->Unlock(level_);
  }
}

void* Texture2D::LockHelper::GetData() {
  if (!locked_) {
    locked_ = texture_->Lock(level_, &data_, &pitch_);
    if (!locked_) {
      O3D_ERROR(texture_->service_locator())
          << "Unable to lock buffer '" << texture_->name() << "'";
    }
  }
  return data_;
}

TextureCUBE::TextureCUBE(ServiceLocator* service_locator,
                         int edge_length,
                         Format format,
                         int levels,
                         bool alpha_is_one,
                         bool resize_to_pot,
                         bool enable_render_surfaces)
    : Texture(service_locator, format, levels, alpha_is_one,
              resize_to_pot, enable_render_surfaces) {
  for (unsigned int i = 0; i < 6; ++i) {
    locked_levels_[i] = 0;
  }
  RegisterReadOnlyParamRef(kEdgeLengthParamName, &edge_length_param_);
  edge_length_param_->set_read_only_value(edge_length);

  ClientInfoManager* client_info_manager =
      service_locator->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    static_cast<int>(image::ComputeMipChainSize(edge_length,
                                                edge_length,
                                                format,
                                                levels)) * 6);
}

TextureCUBE::~TextureCUBE() {
  for (unsigned int i = 0; i < 6; ++i) {
    if (locked_levels_[i] != 0) {
      O3D_ERROR(service_locator())
          << "TextureCUBE \"" << name() << "\" was never unlocked before"
          << "being destroyed.";
      break;  // No need to report it more than once.
    }
  }

  ClientInfoManager* client_info_manager =
      service_locator()->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    -static_cast<int>(image::ComputeMipChainSize(edge_length(),
                                                 edge_length(),
                                                 format(),
                                                 levels()) * 6));
}

ObjectBase::Ref TextureCUBE::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref();
}

void TextureCUBE::DrawImage(const Bitmap& src_img,
                            int src_x, int src_y,
                            int src_width, int src_height,
                            int dst_x, int dst_y,
                            int dst_width, int dst_height,
                            CubeFace dest_face, int dest_mip) {
  DCHECK(src_img.image_data());

  if (dest_face >= NUMBER_OF_FACES) {
    O3D_ERROR(service_locator()) << "Invalid face specification";
    return;
  }

  if (dest_mip < 0 || dest_mip >= levels()) {
    O3D_ERROR(service_locator()) << "Mip out of range";
  }

  unsigned int mip_length = std::max(1, edge_length() >> dest_mip);

  // Clip source and destination rectangles to
  // source and destination bitmaps.
  // if src or dest rectangle is out of boundary,
  // do nothing and return true.
  if (!image::AdjustDrawImageBoundary(&src_x, &src_y,
                                      &src_width, &src_height,
                                      src_img.width(), src_img.height(),
                                      &dst_x, &dst_y,
                                      &dst_width, &dst_height,
                                      mip_length, mip_length)) {
    return;
  }

  unsigned int components = 0;
  // check formats of source and dest images.
  // format of source and dest should be the same.
  if (src_img.format() != format()) {
    O3D_ERROR(service_locator()) << "DrawImage does not support "
                                 << "different formats.";
    return;
  }
  // if src and dest are in the same size and drawImage is copying
  // the entire bitmap on dest image, just perform memcpy.
  if (src_x == 0 && src_y == 0 && dst_x == 0 && dst_y == 0 &&
      src_img.width() == mip_length && src_img.height() == mip_length &&
      static_cast<unsigned int>(src_width) == src_img.width() &&
      static_cast<unsigned int>(src_height) == src_img.height() &&
      static_cast<unsigned int>(dst_width) == mip_length &&
      static_cast<unsigned int>(dst_height) == mip_length) {
    SetRect(dest_face, dest_mip, 0, 0, mip_length, mip_length,
            src_img.image_data(),
            src_img.GetMipPitch(0));
    return;
  }
  if (src_img.format() == Texture::XRGB8 ||
      src_img.format() == Texture::ARGB8) {
    components = 4;
  } else {
    O3D_ERROR(service_locator()) << "DrawImage does not support format: "
                                 << src_img.format() << " unless src and "
                                 << "dest images are in the same size and "
                                 << "copying the entire bitmap";
    return;
  }

  LockHelper helper(this, dest_face, dest_mip);
  uint8* mip_data = helper.GetDataAs<uint8>();
  if (!mip_data) {
    return;
  }

  uint8* src_img_data = src_img.image_data();

  image::LanczosScale(src_img_data, src_x, src_y,
                      src_width, src_height,
                      src_img.width(), src_img.height(),
                      mip_data, helper.pitch(),
                      dst_x, dst_y,
                      dst_width, dst_height,
                      mip_length, mip_length, components);
}

void TextureCUBE::SetFromBitmap(const Bitmap& bitmap) {
  DCHECK(bitmap.image_data());
  if (bitmap.width() != static_cast<unsigned>(edge_length()) ||
      bitmap.height() != static_cast<unsigned>(edge_length()) ||
      bitmap.format() != format() || !bitmap.is_cubemap()) {
    O3D_ERROR(service_locator())
        << "bitmap must be the same format and dimensions as texture";
    return;
  }

  int last_level = std::min<int>(bitmap.num_mipmaps(), levels());
  for (int level = 0; level < last_level; ++level) {
    for (int face = FACE_POSITIVE_X; face < NUMBER_OF_FACES; ++face) {
      SetRect(static_cast<CubeFace>(face), level, 0, 0,
              image::ComputeMipDimension(level, edge_length()),
              image::ComputeMipDimension(level, edge_length()),
              bitmap.GetFaceMipData(static_cast<CubeFace>(face), level),
              bitmap.GetMipPitch(level));
    }
  }
}

void TextureCUBE::GenerateMips(int source_level, int num_levels) {
  if (source_level < 0 || source_level >= levels()) {
    O3D_ERROR(service_locator()) << "source level out of range";
    return;
  }
  if (source_level + num_levels >= levels()) {
    O3D_ERROR(service_locator()) << "num levels out of range";
    return;
  }

  for (int face = FACE_POSITIVE_X; face < NUMBER_OF_FACES; ++face) {
    for (int ii = 0; ii < num_levels; ++ii) {
      int level = source_level + ii;
      TextureCUBE::LockHelper src_helper(
          this, static_cast<TextureCUBE::CubeFace>(face), level);
      TextureCUBE::LockHelper dst_helper(
          this, static_cast<TextureCUBE::CubeFace>(face), level + 1);
      const uint8* src_data = src_helper.GetDataAs<const uint8>();
      if (!src_data) {
        O3D_ERROR(service_locator())
            << "could not lock source texture.";
        return;
      }
      uint8* dst_data = dst_helper.GetDataAs<uint8>();
      if (!dst_data) {
        O3D_ERROR(service_locator())
            << "could not lock destination texture.";
        return;
      }

      unsigned int src_edge_length =
        std::max<unsigned int>(1U, edge_length() >> level);

      image::GenerateMipmap(
          src_edge_length, src_edge_length, format(),
          src_data, src_helper.pitch(),
          dst_data, dst_helper.pitch());
    }
  }
}

TextureCUBE::LockHelper::LockHelper(
    TextureCUBE* texture,
    CubeFace face,
    int level)
    : texture_(texture),
      face_(face),
      level_(level),
      data_(NULL),
      locked_(false) {
}

TextureCUBE::LockHelper::~LockHelper() {
  if (locked_) {
    texture_->Unlock(face_, level_);
  }
}

void* TextureCUBE::LockHelper::GetData() {
  if (!locked_) {
    locked_ = texture_->Lock(face_, level_, &data_, &pitch_);
    if (!locked_) {
      O3D_ERROR(texture_->service_locator())
          << "Unable to lock buffer '" << texture_->name() << "'";
    }
  }
  return data_;
}

}  // namespace o3d
