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


// This file implements the sampler-related GAPI functions on GL.

#include "command_buffer/service/cross/gl/gapi_gl.h"
#include "command_buffer/service/cross/gl/sampler_gl.h"

namespace o3d {
namespace command_buffer {
namespace o3d {

namespace {

// Gets the GL enum corresponding to an addressing mode.
GLenum GLAddressMode(sampler::AddressingMode o3d_mode) {
  switch (o3d_mode) {
    case sampler::kWrap:
      return GL_REPEAT;
    case sampler::kMirrorRepeat:
      return GL_MIRRORED_REPEAT;
    case sampler::kClampToEdge:
      return GL_CLAMP_TO_EDGE;
    case sampler::kClampToBorder:
      return GL_CLAMP_TO_BORDER;
    default:
      DLOG(FATAL) << "Not Reached";
      return GL_REPEAT;
  }
}

// Gets the GL enum for the minification filter based on the command buffer min
// and mip filtering modes.
GLenum GLMinFilter(sampler::FilteringMode min_filter,
                   sampler::FilteringMode mip_filter) {
  switch (min_filter) {
    case sampler::kPoint:
      if (mip_filter == sampler::kNone)
        return GL_NEAREST;
      else if (mip_filter == sampler::kPoint)
        return GL_NEAREST_MIPMAP_NEAREST;
      else if (mip_filter == sampler::kLinear)
        return GL_NEAREST_MIPMAP_LINEAR;
    case sampler::kLinear:
      if (mip_filter == sampler::kNone)
        return GL_LINEAR;
      else if (mip_filter == sampler::kPoint)
        return GL_LINEAR_MIPMAP_NEAREST;
      else if (mip_filter == sampler::kLinear)
        return GL_LINEAR_MIPMAP_LINEAR;
    default:
      DLOG(FATAL) << "Not Reached";
      return GL_LINEAR_MIPMAP_NEAREST;
  }
}

// Gets the GL enum for the magnification filter based on the command buffer mag
// filtering mode.
GLenum GLMagFilter(sampler::FilteringMode mag_filter) {
  switch (mag_filter) {
    case sampler::kPoint:
      return GL_NEAREST;
    case sampler::kLinear:
      return GL_LINEAR;
    default:
      DLOG(FATAL) << "Not Reached";
      return GL_LINEAR;
  }
}

// Gets the GL enum representing the GL target based on the texture type.
GLenum GLTextureTarget(texture::Type type) {
  switch (type) {
    case texture::kTexture2d:
      return GL_TEXTURE_2D;
    case texture::kTexture3d:
      return GL_TEXTURE_3D;
    case texture::kTextureCube:
      return GL_TEXTURE_CUBE_MAP;
    default:
      DLOG(FATAL) << "Not Reached";
      return GL_TEXTURE_2D;
  }
}

}  // anonymous namespace

SamplerGL::SamplerGL()
    : texture_id_(kInvalidResource),
      gl_texture_(0) {
  SetStates(sampler::kClampToEdge,
            sampler::kClampToEdge,
            sampler::kClampToEdge,
            sampler::kLinear,
            sampler::kLinear,
            sampler::kPoint,
            1);
  RGBA black = {0, 0, 0, 1};
  SetBorderColor(black);
}

bool SamplerGL::ApplyStates(GAPIGL *gapi) {
  DCHECK(gapi);
  TextureGL *texture = gapi->GetTexture(texture_id_);
  if (!texture) {
    gl_texture_ = 0;
    return false;
  }
  GLenum target = GLTextureTarget(texture->type());
  gl_texture_ = texture->gl_texture();
  glBindTexture(target, gl_texture_);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, gl_wrap_s_);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, gl_wrap_t_);
  glTexParameteri(target, GL_TEXTURE_WRAP_R, gl_wrap_r_);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, gl_min_filter_);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, gl_mag_filter_);
  glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_max_anisotropy_);
  glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, gl_border_color_);
  return true;
}

void SamplerGL::SetStates(sampler::AddressingMode addressing_u,
                          sampler::AddressingMode addressing_v,
                          sampler::AddressingMode addressing_w,
                          sampler::FilteringMode mag_filter,
                          sampler::FilteringMode min_filter,
                          sampler::FilteringMode mip_filter,
                          unsigned int max_anisotropy) {
  // These are validated in GAPIDecoder.cc
  DCHECK_NE(mag_filter, sampler::kNone);
  DCHECK_NE(min_filter, sampler::kNone);
  DCHECK_GT(max_anisotropy, 0U);
  gl_wrap_s_ = GLAddressMode(addressing_u);
  gl_wrap_t_ = GLAddressMode(addressing_v);
  gl_wrap_r_ = GLAddressMode(addressing_w);
  gl_mag_filter_ = GLMagFilter(mag_filter);
  gl_min_filter_ = GLMinFilter(min_filter, mip_filter);
  gl_max_anisotropy_ = max_anisotropy;
}

void SamplerGL::SetBorderColor(const RGBA &color) {
  gl_border_color_[0] = color.red;
  gl_border_color_[1] = color.green;
  gl_border_color_[2] = color.blue;
  gl_border_color_[3] = color.alpha;
}

parse_error::ParseError GAPIGL::CreateSampler(
    ResourceId id) {
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  samplers_.Assign(id, new SamplerGL());
  return parse_error::kParseNoError;
}

// Destroys the Sampler resource.
parse_error::ParseError GAPIGL::DestroySampler(ResourceId id) {
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  return samplers_.Destroy(id) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

parse_error::ParseError GAPIGL::SetSamplerStates(
    ResourceId id,
    sampler::AddressingMode addressing_u,
    sampler::AddressingMode addressing_v,
    sampler::AddressingMode addressing_w,
    sampler::FilteringMode mag_filter,
    sampler::FilteringMode min_filter,
    sampler::FilteringMode mip_filter,
    unsigned int max_anisotropy) {
  SamplerGL *sampler = samplers_.Get(id);
  if (!sampler)
    return parse_error::kParseInvalidArguments;
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  sampler->SetStates(addressing_u, addressing_v, addressing_w,
                     mag_filter, min_filter, mip_filter, max_anisotropy);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIGL::SetSamplerBorderColor(
    ResourceId id,
    const RGBA &color) {
  SamplerGL *sampler = samplers_.Get(id);
  if (!sampler)
    return parse_error::kParseInvalidArguments;
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  sampler->SetBorderColor(color);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIGL::SetSamplerTexture(
    ResourceId id,
    ResourceId texture_id) {
  SamplerGL *sampler = samplers_.Get(id);
  if (!sampler)
    return parse_error::kParseInvalidArguments;
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  sampler->SetTexture(texture_id);
  return parse_error::kParseNoError;
}

}  // namespace o3d
}  // namespace command_buffer
}  // namespace o3d
