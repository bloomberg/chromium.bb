// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/gl/gl_surface_format.h"

namespace gl {

GLSurfaceFormat::GLSurfaceFormat() {
}

GLSurfaceFormat::GLSurfaceFormat(SurfacePixelLayout layout) {
  is_default_ = false;
  pixel_layout_ = layout;
}

GLSurfaceFormat::GLSurfaceFormat(const GLSurfaceFormat& other) = default;

GLSurfaceFormat::~GLSurfaceFormat() {
}

bool GLSurfaceFormat::IsSurfaceless() {
  return is_surfaceless_;
}

GLSurfaceFormat::SurfacePixelLayout GLSurfaceFormat::GetPixelLayout() {
  return pixel_layout_;
}

void GLSurfaceFormat::SetDefaultPixelLayout(SurfacePixelLayout layout) {
  if (pixel_layout_ == PIXEL_LAYOUT_DONT_CARE &&
      layout != PIXEL_LAYOUT_DONT_CARE) {
    pixel_layout_ = layout;
    is_default_ = false;
  }
}

void GLSurfaceFormat::SetRGB565() {
  red_bits_ = blue_bits_ = 5;
  green_bits_ = 6;
  alpha_bits_ = 0;
  is_default_ = false;
}

void GLSurfaceFormat::SetIsSurfaceless() {
  is_surfaceless_ = true;
  is_default_ = false;
}

static int GetValue(int num, int default_value) {
  return num == -1 ? default_value : num;
}

static int GetBitSize(int num) {
  return GetValue(num, 8);
}


bool GLSurfaceFormat::IsDefault() {
  return is_default_;
}

bool GLSurfaceFormat::IsCompatible(GLSurfaceFormat other) {
  if (IsDefault() && other.IsDefault()) return true;

  if (GetBitSize(red_bits_) == GetBitSize(other.red_bits_) &&
      GetBitSize(green_bits_) == GetBitSize(other.green_bits_) &&
      GetBitSize(blue_bits_) == GetBitSize(other.blue_bits_) &&
      GetBitSize(alpha_bits_) == GetBitSize(other.alpha_bits_) &&
      GetValue(stencil_bits_, 8) == GetValue(other.stencil_bits_, 8) &&
      GetValue(depth_bits_, 24) == GetValue(other.depth_bits_, 24) &&
      GetValue(samples_, 0) == GetValue(other.samples_, 0) &&
      is_surfaceless_ == other.is_surfaceless_ &&
      pixel_layout_ == other.pixel_layout_) {
    return true;
  }
  return false;
}

void GLSurfaceFormat::SetDepthBits(int bits) {
  if (bits != -1) {
    depth_bits_ = bits;
    is_default_ = false;
  }
}

int GLSurfaceFormat::GetDepthBits() {
  return depth_bits_;
}

void GLSurfaceFormat::SetStencilBits(int bits) {
  if (bits != -1) {
    stencil_bits_ = bits;
    is_default_ = false;
  }
}

int GLSurfaceFormat::GetStencilBits() {
  return stencil_bits_;
}

void GLSurfaceFormat::SetSamples(int num) {
  if (num != -1) {
    samples_ = num;
    is_default_ = false;
  }
}

int GLSurfaceFormat::GetSamples() {
  return samples_;
}

int GLSurfaceFormat::GetBufferSize() {
  int bits = GetBitSize(red_bits_) + GetBitSize(green_bits_) +
      GetBitSize(blue_bits_) + GetBitSize(alpha_bits_);
  if (bits <= 16) {
    return 16;
  } else if (bits <= 32) {
    return 32;
  }
  NOTREACHED();
  return 64;
}

}  // namespace gl
