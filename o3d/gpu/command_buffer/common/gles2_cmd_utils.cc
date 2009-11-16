// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace command_buffer {
namespace gles2 {

GLsizei GLES2Util::GLGetNumValuesReturned(GLenum id) const {
  switch (id) {
    // -- glGetBooleanv, glGetFloatv, glGetIntergerv
    case GL_ACTIVE_TEXTURE:
      return 1;
    case GL_ALIASED_LINE_WIDTH_RANGE:
      return 2;
    case GL_ALIASED_POINT_SIZE_RANGE:
      return 1;
    case GL_ALPHA_BITS:
      return 1;
    case GL_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_BLEND:
      return 1;
    case GL_BLEND_COLOR:
      return 4;
    case GL_BLEND_DST_ALPHA:
      return 1;
    case GL_BLEND_DST_RGB:
      return 1;
    case GL_BLEND_EQUATION_ALPHA:
      return 1;
    case GL_BLEND_EQUATION_RGB:
      return 1;
    case GL_BLEND_SRC_ALPHA:
      return 1;
    case GL_BLEND_SRC_RGB:
      return 1;
    case GL_BLUE_BITS:
      return 1;
    case GL_COLOR_CLEAR_VALUE:
      return 4;
    case GL_COLOR_WRITEMASK:
      return 4;
    case GL_COMPRESSED_TEXTURE_FORMATS:
      return num_compressed_texture_formats_;
    case GL_CULL_FACE:
      return 1;
    case GL_CULL_FACE_MODE:
      return 1;
    case GL_CURRENT_PROGRAM:
      return 1;
    case GL_DEPTH_BITS:
      return 1;
    case GL_DEPTH_CLEAR_VALUE:
      return 1;
    case GL_DEPTH_FUNC:
      return 1;
    case GL_DEPTH_RANGE:
      return 2;
    case GL_DEPTH_TEST:
      return 1;
    case GL_DEPTH_WRITEMASK:
      return 1;
    case GL_DITHER:
      return 1;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_FRAMEBUFFER_BINDING:
      return 1;
    case GL_FRONT_FACE:
      return 1;
    case GL_GENERATE_MIPMAP_HINT:
      return 1;
    case GL_GREEN_BITS:
      return 1;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      return 1;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      return 1;
    case GL_LINE_WIDTH:
      return 1;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return 1;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return 1;
    case GL_MAX_RENDERBUFFER_SIZE:
      return 1;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_TEXTURE_SIZE:
      return 1;
    case GL_MAX_VARYING_VECTORS:
      return 1;
    case GL_MAX_VERTEX_ATTRIBS:
      return 1;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      return 1;
    case GL_MAX_VIEWPORT_DIMS:
      return 2;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      return 1;
    case GL_PACK_ALIGNMENT:
      return 1;
    case GL_POLYGON_OFFSET_FACTOR:
      return 1;
    case GL_POLYGON_OFFSET_FILL:
      return 1;
    case GL_POLYGON_OFFSET_UNITS:
      return 1;
    case GL_RED_BITS:
      return 1;
    case GL_RENDERBUFFER_BINDING:
      return 1;
    case GL_SAMPLE_BUFFERS:
      return 1;
    case GL_SAMPLE_COVERAGE_INVERT:
      return 1;
    case GL_SAMPLE_COVERAGE_VALUE:
      return 1;
    case GL_SAMPLES:
      return 1;
    case GL_SCISSOR_BOX:
      return 4;
    case GL_SCISSOR_TEST:
      return 1;
    case GL_SHADER_COMPILER:
      return 1;
    case GL_STENCIL_BACK_FAIL:
      return 1;
    case GL_STENCIL_BACK_FUNC:
      return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      return 1;
    case GL_STENCIL_BACK_REF:
      return 1;
    case GL_STENCIL_BACK_VALUE_MASK:
      return 1;
    case GL_STENCIL_BACK_WRITEMASK:
      return 1;
    case GL_STENCIL_BITS:
      return 1;
    case GL_STENCIL_CLEAR_VALUE:
      return 1;
    case GL_STENCIL_FAIL:
      return 1;
    case GL_STENCIL_FUNC:
      return 1;
    case GL_STENCIL_PASS_DEPTH_FAIL:
      return 1;
    case GL_STENCIL_PASS_DEPTH_PASS:
      return 1;
    case GL_STENCIL_REF:
      return 1;
    case GL_STENCIL_TEST:
      return 1;
    case GL_STENCIL_VALUE_MASK:
      return 1;
    case GL_STENCIL_WRITEMASK:
      return 1;
    case GL_SUBPIXEL_BITS:
      return 1;
    case GL_TEXTURE_BINDING_2D:
      return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      return 1;
    case GL_UNPACK_ALIGNMENT:
      return 1;
    case GL_VIEWPORT:
      return 4;

    // -- glGetBufferParameteriv
    case GL_BUFFER_SIZE:
      return 1;
    case GL_BUFFER_USAGE:
      return 1;

    // -- glGetFramebufferAttachmentParameteriv
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      return 1;

    // -- glGetFramebufferAttachmentParameteriv
    case GL_DELETE_STATUS:
      return 1;
    case GL_LINK_STATUS:
      return 1;
    case GL_VALIDATE_STATUS:
      return 1;
    case GL_INFO_LOG_LENGTH:
      return 1;
    case GL_ATTACHED_SHADERS:
      return 1;
    case GL_ACTIVE_ATTRIBUTES:
      return 1;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      return 1;
    case GL_ACTIVE_UNIFORMS:
      return 1;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      return 1;


    // -- glGetRenderbufferAttachmentParameteriv
    case GL_RENDERBUFFER_WIDTH:
      return 1;
    case GL_RENDERBUFFER_HEIGHT:
      return 1;
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      return 1;
    case GL_RENDERBUFFER_RED_SIZE:
      return 1;
    case GL_RENDERBUFFER_GREEN_SIZE:
      return 1;
    case GL_RENDERBUFFER_BLUE_SIZE:
      return 1;
    case GL_RENDERBUFFER_ALPHA_SIZE:
      return 1;
    case GL_RENDERBUFFER_DEPTH_SIZE:
      return 1;
    case GL_RENDERBUFFER_STENCIL_SIZE:
      return 1;

    // -- glGetShaderiv
    case GL_SHADER_TYPE:
      return 1;
    // Already defined under glGetFramebufferAttachemntParameteriv.
    // case GL_DELETE_STATUS:
    //   return 1;
    case GL_COMPILE_STATUS:
      return 1;
    // Already defined under glGetFramebufferAttachemntParameteriv.
    // case GL_INFO_LOG_LENGTH:
    //   return 1;
    case GL_SHADER_SOURCE_LENGTH:
      return 1;

    // -- glGetTexParameterfv, glGetTexParameteriv
    case GL_TEXTURE_MAG_FILTER:
      return 1;
    case GL_TEXTURE_MIN_FILTER:
      return 1;
    case GL_TEXTURE_WRAP_S:
      return 1;
    case GL_TEXTURE_WRAP_T:
      return 1;

    // -- glGetVertexAttribfv, glGetVertexAttribiv
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      return 1;
    case GL_CURRENT_VERTEX_ATTRIB:
      return 4;

    // bad enum
    default:
      return 0;
  }
}

namespace {

// Return the number of elements per group of a specified format.
GLint ElementsPerGroup(GLenum format, GLenum type) {
  switch (type) {
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
       return 1;
    default:
       break;
    }

    switch (format) {
    case GL_RGB:
       return 3;
    case GL_LUMINANCE_ALPHA:
       return 2;
    case GL_RGBA:
       return 4;
    case GL_ALPHA:
    case GL_LUMINANCE:
       return 1;
    default:
       return 0;
  }
}

// Return the number of bytes per element, based on the element type.
GLint BytesPerElement(GLenum type) {
  switch (type) {
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
       return 2;
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
       return 1;
    default:
       return 0;
  }
}

}  // anonymous namespace

// Returns the amount of data glTexImage2D or glTexSubImage2D will access.
uint32 GLES2Util::ComputeImageDataSize(
    GLsizei width, GLsizei height, GLenum format, GLenum type,
    GLint unpack_alignment) {
  uint32 bytes_per_group = BytesPerElement(ElementsPerGroup(format, type));
  uint32 row_size = width * bytes_per_group;
  if (height > 1) {
    uint32 padded_row_size = ((row_size + unpack_alignment - 1) /
                              unpack_alignment) * unpack_alignment;
    return height - 1 * padded_row_size + row_size;
  }
  return height * row_size;
}

}  // namespace gles2
}  // namespace command_buffer

