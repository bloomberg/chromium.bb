// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H

// The GPU Service needs to include the OS's GL headers.
#ifdef GPU_SERVICE
#include <GL/glew.h>
#if defined(OS_WIN)
#include <GL/wglew.h>
#endif
#else
#include <GLES2/gl2.h>
#endif
#include "base/basictypes.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/common/bitfield_helpers.h"

namespace command_buffer {
namespace gles2 {

// Utilties for GLES2 support.
class GLES2Util {
 public:
  explicit GLES2Util(
      GLsizei num_compressed_texture_formats)
      : num_compressed_texture_formats_(num_compressed_texture_formats) {
  }

  // Gets the number of values a particular id will return when a glGet
  // function is called. If 0 is returned the id is invalid.
  GLsizei GLGetNumValuesReturned(GLenum id) const;

  // Computes the size of image data for TexImage2D and TexSubImage2D.
  static uint32 GLES2Util::ComputeImageDataSize(
    GLsizei width, GLsizei height, GLenum format, GLenum type,
    GLint unpack_alignment);

 private:
  GLsizei num_compressed_texture_formats_;
};

}  // namespace gles2
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H

