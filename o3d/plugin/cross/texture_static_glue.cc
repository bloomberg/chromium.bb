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

#include <vector>
#include "core/cross/pointer_utils.h"
#include "core/cross/error.h"
#include "core/cross/math_utilities.h"
#include "core/cross/texture.h"
#include "core/cross/image_utils.h"

namespace {

void SetRectCheck(o3d::Texture* self,
                  void* data,
                  int pitch,
                  int destination_x,
                  int destination_y,
                  int texture_width,
                  int texture_height,
                  int source_width,
                  int source_height,
                  const std::vector<float>& values) {
  unsigned num_components;
  unsigned swizzle[4] = {2, 1, 0, 3};
  switch (self->format()) {
    case o3d::Texture::XRGB8:
      num_components = 3;
      break;
    case o3d::Texture::R32F:
      swizzle[0] = 0;
      num_components = 1;
      break;
    case o3d::Texture::ARGB8:
    case o3d::Texture::ABGR16F:
      num_components = 4;
      break;
    case o3d::Texture::ABGR32F: {
      num_components = 4;
      const o3d::Texture::RGBASwizzleIndices& indices =
         self->GetABGR32FSwizzleIndices();
      for (int ii = 0; ii < 4; ++ii) {
        swizzle[ii] = indices[ii];
      }
      break;
    }
    default:
      DCHECK(false);
      return;
  }

  // clip
  int source_x = 0;
  int source_y = 0;
  int copy_width = source_width;
  int copy_height = source_height;

  if (destination_x < 0) {
    copy_width += destination_x;
    source_x -= destination_x;
    destination_x = 0;
  }
  if (destination_x + copy_width > static_cast<int>(texture_width)) {
    copy_width -= destination_x + copy_width - texture_width;
  }

  if (destination_y < 0) {
    copy_height += destination_y;
    source_y -= destination_y;
    destination_y = 0;
  }
  if (destination_y + copy_height > static_cast<int>(texture_height)) {
    copy_height -= destination_y + copy_height - texture_height;
  }

  if (copy_width <= 0 || copy_height <= 0) {
    return;
  }

  const float* source =
      &values[0] +
      (source_y * source_width + source_x) * num_components;
  unsigned source_stride = (source_width - copy_width) * num_components;
  switch (self->format()) {
    case o3d::Texture::ABGR16F: {
      uint16* dest_line =
          static_cast<uint16*>(data) +
          (destination_y * texture_width + destination_x) * num_components;
      for (; copy_height > 0; --copy_height) {
        uint16* destination = dest_line;
        for (int xx = 0; xx < copy_width; ++xx) {
          for (unsigned element = 0; element < num_components; ++element) {
            destination[element] = Vectormath::Aos::FloatToHalf(
                source[swizzle[element]]);
          }
          destination += num_components;
          source += num_components;
        }
        dest_line = o3d::AddPointerOffset<uint16*>(dest_line, pitch);
        source += source_stride;
      }
      break;
    }
    case o3d::Texture::R32F:
    case o3d::Texture::ABGR32F: {
      float* dest_line =
        static_cast<float*>(data) +
        (destination_y * texture_width + destination_x) * num_components;
      for (; copy_height > 0; --copy_height) {
        float* destination = dest_line;
        for (int xx = 0; xx < copy_width; ++xx) {
          for (unsigned element = 0; element < num_components; ++element) {
            destination[element] = source[swizzle[element]];
          }
          destination += num_components;
          source += num_components;
        }
        dest_line = o3d::AddPointerOffset<float*>(dest_line, pitch);
        source += source_stride;
      }
      break;
    }
    default: {
      uint8* dest_line =
          static_cast<uint8*>(data) +
         (destination_y * texture_width + destination_x) * 4;
      for (; copy_height > 0; --copy_height) {
        uint8* destination = dest_line;
        for (int xx = 0; xx < copy_width; ++xx) {
          destination[0] = static_cast<unsigned char>(
              source[swizzle[0]] * 255.0f);
          destination[1] = static_cast<unsigned char>(
              source[swizzle[1]] * 255.0f);
          destination[2] = static_cast<unsigned char>(
              source[swizzle[2]] * 255.0f);
          destination[3] = (num_components == 4) ?
              static_cast<unsigned char>(source[swizzle[3]] * 255.0f) : 255;
          destination += 4;
          source += num_components;
        }
        dest_line = o3d::AddPointerOffset<uint8*>(dest_line, pitch);
        source += source_stride;
      }
      break;
    }
  }
}

void SetRectCheck2D(o3d::Texture2D* self,
                    int level,
                    int destination_x,
                    int destination_y,
                    int source_width,
                    const std::vector<float>& values,
                    bool check_needed) {
  if (level < 0 || level >= self->levels()) {
    O3D_ERROR(self->service_locator())
        << "level (" << level << " out of range";
    return;
  }
  if (values.empty() || source_width <= 0) {
    return;
  }
  unsigned num_values = values.size();
  unsigned texture_width = std::max(self->width() >> level, 1);
  unsigned texture_height = std::max(self->height() >> level, 1);
  unsigned num_components;
  switch (self->format()) {
    case o3d::Texture::XRGB8:
      num_components = 3;
      break;
    case o3d::Texture::R32F:
      num_components = 1;
      break;
    case o3d::Texture::ARGB8:
    case o3d::Texture::ABGR16F:
      num_components = 4;
      break;
    case o3d::Texture::ABGR32F: {
      num_components = 4;
      break;
    }
    default:
      O3D_ERROR(self->service_locator())
        << "Texture::Set not supported for this type of texture";
      return;
  }
  if (num_values % num_components != 0) {
    O3D_ERROR(self->service_locator())
      << "The number of elements passed in must be a multiple of "
      << num_components;
  }
  unsigned num_elements = num_values / num_components;
  if (num_elements % source_width != 0) {
    O3D_ERROR(self->service_locator())
      << "The number of elements passed in must be a multiple of the "
      << "width";
    return;
  }
  unsigned source_height = num_elements / source_width;
  if (check_needed) {
    unsigned needed = num_components * texture_width * texture_height;
    if (num_values != needed) {
      O3D_ERROR(self->service_locator())
        << "needed " << needed << " values but " << num_values
        << " passed in.";
      return;
    }
  }
  o3d::Texture2D::LockHelper helper(self, level);
  void* data = helper.GetData();
  if (!data) {
    O3D_ERROR(self->service_locator()) << "could not lock texture";
    return;
  }

  SetRectCheck(self, data, helper.pitch(),
               destination_x, destination_y,
               texture_width, texture_height,
               source_width, source_height,
               values);
}

void SetRectCheckCUBE(o3d::TextureCUBE* self,
                      o3d::TextureCUBE::CubeFace face,
                      int level,
                      int destination_x,
                      int destination_y,
                      int source_width,
                      const std::vector<float>& values,
                      bool check_needed) {
  if (level < 0 || level >= self->levels()) {
    O3D_ERROR(self->service_locator())
        << "level (" << level << " out of range";
    return;
  }
  if (values.empty() || source_width <= 0) {
    return;
  }
  unsigned num_values = values.size();
  unsigned texture_width = std::max(self->edge_length() >> level, 1);
  unsigned texture_height = texture_width;
  unsigned num_components;
  switch (self->format()) {
    case o3d::Texture::XRGB8:
      num_components = 3;
      break;
    case o3d::Texture::R32F:
      num_components = 1;
      break;
    case o3d::Texture::ARGB8:
    case o3d::Texture::ABGR16F:
      num_components = 4;
      break;
    case o3d::Texture::ABGR32F: {
      num_components = 4;
      break;
    }
    default:
      O3D_ERROR(self->service_locator())
        << "Texture::Set not supported for this type of texture";
      return;
  }
  if (num_values % num_components != 0) {
    O3D_ERROR(self->service_locator())
      << "The number of elements passed in must be a multiple of "
      << num_components;
  }
  unsigned num_elements = num_values / num_components;
  if (num_elements % source_width != 0) {
    O3D_ERROR(self->service_locator())
      << "The number of elements passed in must be a multiple of the "
      << "width";
    return;
  }
  unsigned source_height = num_elements / source_width;
  if (check_needed) {
    unsigned needed = num_components * texture_width * texture_height;
    if (num_values != needed) {
      O3D_ERROR(self->service_locator())
        << "needed " << needed << " values but " << num_values
        << " passed in.";
      return;
    }
  }
  o3d::TextureCUBE::LockHelper helper(self, face, level);
  void* data = helper.GetData();
  if (!data) {
    O3D_ERROR(self->service_locator()) << "could not lock texture";
    return;
  }

  SetRectCheck(self, data, helper.pitch(),
               destination_x, destination_y,
               texture_width, texture_height,
               source_width, source_height,
               values);
}

// Assumes dst points to width * height * num_components floats.
void GetRect(o3d::Texture* self,
             const void* src_data,
             int src_pitch,
             int x,
             int y,
             int width,
             int height,
             float* dst) {
  unsigned num_components;
  unsigned swizzle[4] = {2, 1, 0, 3};
  switch (self->format()) {
    case o3d::Texture::XRGB8:
      num_components = 3;
      break;
    case o3d::Texture::R32F:
      swizzle[0] = 0;
      num_components = 1;
      break;
    case o3d::Texture::ARGB8:
    case o3d::Texture::ABGR16F:
      num_components = 4;
      break;
    case o3d::Texture::ABGR32F: {
      num_components = 4;
      const o3d::Texture::RGBASwizzleIndices& indices =
         self->GetABGR32FSwizzleIndices();
      for (int ii = 0; ii < 4; ++ii) {
        swizzle[ii] = indices[ii];
      }
      break;
    }
    default:
      DCHECK(false);
      return;
  }

  switch (self->format()) {
    case o3d::Texture::ABGR16F: {
      uint16* src_line = o3d::PointerFromVoidPointer<uint16*>(
          src_data,
          (y * src_pitch)) + x * num_components;
      for (; height > 0; --height) {
        uint16* src = src_line;
        for (int xx = 0; xx < width; ++xx) {
          for (unsigned element = 0; element < num_components; ++element) {
            dst[swizzle[element]] = Vectormath::Aos::HalfToFloat(src[element]);
          }
          dst += num_components;
          src += num_components;
        }
        src_line = o3d::AddPointerOffset<uint16*>(src_line, src_pitch);
      }
      break;
    }
    case o3d::Texture::R32F:
    case o3d::Texture::ABGR32F: {
      float* src_line = o3d::PointerFromVoidPointer<float*>(
          src_data,
          (y * src_pitch)) + x * num_components;
      for (; height > 0; --height) {
        float* src = src_line;
        for (int xx = 0; xx < width; ++xx) {
          for (unsigned element = 0; element < num_components; ++element) {
            dst[swizzle[element]] = src[element];
          }
          dst += num_components;
          src += num_components;
        }
        src_line = o3d::AddPointerOffset<float*>(src_line, src_pitch);
      }
      break;
    }
    default: {
      uint8* src_line = o3d::PointerFromVoidPointer<uint8*>(
          src_data,
          (y * src_pitch)) + x * num_components;
      for (; height > 0; --height) {
        uint8* src = src_line;
        for (int xx = 0; xx < width; ++xx) {
          dst[swizzle[0]] = static_cast<float>(src[0]) / 255.0f;
          dst[swizzle[1]] = static_cast<float>(src[1]) / 255.0f;
          dst[swizzle[2]] = static_cast<float>(src[2]) / 255.0f;
          if (num_components == 4) {
            dst[swizzle[3]] = static_cast<float>(src[3]) / 255.0f;
          }
          dst += num_components;
          src += 4;
        }
        src_line = o3d::AddPointerOffset<uint8*>(src_line, src_pitch);
      }
      break;
    }
  }
}

}  // anonymous namespace

namespace glue {
namespace namespace_o3d {
namespace class_Texture2D {

void userglue_method_SetRect(o3d::Texture2D* self,
                             int level,
                             int destination_x,
                             int destination_y,
                             int source_width,
                             const std::vector<float>& values) {
  SetRectCheck2D(self,
                 level,
                 destination_x,
                 destination_y,
                 source_width,
                 values,
                 false);
}
void userglue_method_Set(o3d::Texture2D* self,
                         int level,
                         const std::vector<float>& values) {
  SetRectCheck2D(self, level, 0, 0, self->width(), values, true);
}
std::vector<float> userglue_method_GetRect(o3d::Texture2D* self,
                                           int level,
                                           int x,
                                           int y,
                                           int width,
                                           int height) {
  std::vector<float> empty;
  if (level < 0 || level >= self->levels()) {
    O3D_ERROR(self->service_locator())
        << "level (" << level << " out of range";
    return empty;
  }
  if (width <= 0 || height <= 0) {
    O3D_ERROR(self->service_locator())
        << "width and height must be positive";
    return empty;
  }

  int mip_width =
      static_cast<int>(o3d::image::ComputeMipDimension(level, self->width()));
  int mip_height =
      static_cast<int>(o3d::image::ComputeMipDimension(level, self->height()));

  if (x < 0 || x + width > mip_width || y < 0 || y + height > mip_height) {
    O3D_ERROR(self->service_locator()) << "area out of range";
    return empty;
  }

  unsigned num_components;
  switch (self->format()) {
    case o3d::Texture::XRGB8:
      num_components = 3;
      break;
    case o3d::Texture::R32F:
      num_components = 1;
      break;
    case o3d::Texture::ARGB8:
    case o3d::Texture::ABGR16F:
      num_components = 4;
      break;
    case o3d::Texture::ABGR32F: {
      num_components = 4;
      break;
    }
    default:
      O3D_ERROR(self->service_locator())
        << "Texture::Set not supported for this type of texture";
      return empty;
  }
  o3d::Texture2D::LockHelper helper(self, level);
  void* data = helper.GetData();
  if (!data) {
    O3D_ERROR(self->service_locator()) << "could not lock texture";
    return empty;
  }

  std::vector<float> values(width * height * num_components, 0);
  GetRect(self, data, helper.pitch(), x, y, width, height, &values[0]);
  return values;
}

}  // namespace class_Texture2D

namespace class_TextureCUBE {

void userglue_method_SetRect(o3d::TextureCUBE* self,
                             o3d::TextureCUBE::CubeFace face,
                             int level,
                             int destination_x,
                             int destination_y,
                             int source_width,
                             const std::vector<float>& values) {
  SetRectCheckCUBE(self,
                   face,
                   level,
                   destination_x,
                   destination_y,
                   source_width,
                   values,
                   false);
}
void userglue_method_Set(o3d::TextureCUBE* self,
                         o3d::TextureCUBE::CubeFace face,
                         int level,
                         const std::vector<float>& values) {
  SetRectCheckCUBE(self, face, level, 0, 0, self->edge_length(), values, true);
}
std::vector<float> userglue_method_GetRect(o3d::TextureCUBE* self,
                                           o3d::TextureCUBE::CubeFace face,
                                           int level,
                                           int x,
                                           int y,
                                           int width,
                                           int height) {
  std::vector<float> empty;
  if (level < 0 || level >= self->levels()) {
    O3D_ERROR(self->service_locator())
        << "level (" << level << " out of range";
    return empty;
  }
  if (width <= 0 || height <= 0) {
    O3D_ERROR(self->service_locator())
        << "width and height must be positive";
    return empty;
  }

  int mip_length = static_cast<int>(o3d::image::ComputeMipDimension(
      level, self->edge_length()));

  if (x < 0 || x + width > mip_length || y < 0 || y + height > mip_length) {
    O3D_ERROR(self->service_locator()) << "area out of range";
    return empty;
  }

  unsigned num_components;
  switch (self->format()) {
    case o3d::Texture::XRGB8:
      num_components = 3;
      break;
    case o3d::Texture::R32F:
      num_components = 1;
      break;
    case o3d::Texture::ARGB8:
    case o3d::Texture::ABGR16F:
      num_components = 4;
      break;
    case o3d::Texture::ABGR32F: {
      num_components = 4;
      break;
    }
    default:
      O3D_ERROR(self->service_locator())
        << "Texture::Set not supported for this type of texture";
      return empty;
  }
  o3d::TextureCUBE::LockHelper helper(self, face, level);
  void* data = helper.GetData();
  if (!data) {
    O3D_ERROR(self->service_locator()) << "could not lock texture";
    return empty;
  }

  std::vector<float> values(width * height * num_components, 0);
  GetRect(self, data, helper.pitch(), x, y, width, height, &values[0]);
  return values;
}

}  // namespace class_TextureCUBE

}  // namespace namespace_o3d
}  // namespace glue

