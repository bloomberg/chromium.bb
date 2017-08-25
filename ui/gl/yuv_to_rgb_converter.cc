// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/yuv_to_rgb_converter.h"

#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/color_transform.h"
#include "ui/gl/gl_helper.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

namespace gl {
namespace {

const char kVertexHeaderCompatiblityProfile[] =
    "#version 110\n"
    "#define ATTRIBUTE attribute\n"
    "#define VARYING varying\n";

const char kVertexHeaderCoreProfile[] =
    "#version 150\n"
    "#define ATTRIBUTE in\n"
    "#define VARYING out\n";

const char kFragmentHeaderCompatiblityProfile[] =
    "#version 110\n"
    "#extension GL_ARB_texture_rectangle : require\n"
    "#define VARYING varying\n"
    "#define FRAGCOLOR gl_FragColor\n"
    "#define TEX texture2DRect\n";

const char kFragmentHeaderCoreProfile[] =
    "#version 150\n"
    "#define VARYING in\n"
    "#define TEX texture\n"
    "#define FRAGCOLOR frag_color\n"
    "out vec4 FRAGCOLOR;\n";

// clang-format off
const char kVertexShader[] =
STRINGIZE(
  ATTRIBUTE vec2 a_position;
  uniform vec2 a_texScale;
  VARYING vec2 v_texCoord;
  void main() {
    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
    v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5 * a_texScale;
  }
);

const char kFragmentShader[] =
STRINGIZE(
  uniform sampler2DRect a_y_texture;
  uniform sampler2DRect a_uv_texture;
  VARYING vec2 v_texCoord;
  void main() {
    vec3 yuv = vec3(
        TEX(a_y_texture, v_texCoord).r,
        TEX(a_uv_texture, v_texCoord * 0.5).rg);
    FRAGCOLOR = vec4(DoColorConversion(yuv), 1.0);
  }
);
// clang-format on

}  // namespace

YUVToRGBConverter::YUVToRGBConverter(const GLVersionInfo& gl_version_info,
                                     const gfx::ColorSpace color_space) {
  std::unique_ptr<gfx::ColorTransform> color_transform =
      gfx::ColorTransform::NewColorTransform(
          color_space, color_space.GetAsFullRangeRGB(),
          gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
  DCHECK(color_transform->CanGetShaderSource());
  std::string do_color_conversion = color_transform->GetShaderSource();

  bool use_core_profile = gl_version_info.is_desktop_core_profile;
  glGenFramebuffersEXT(1, &framebuffer_);
  vertex_buffer_ = GLHelper::SetupQuadVertexBuffer();
  vertex_shader_ = GLHelper::LoadShader(
      GL_VERTEX_SHADER,
      base::StringPrintf("%s\n%s",
                         use_core_profile ? kVertexHeaderCoreProfile
                                          : kVertexHeaderCompatiblityProfile,
                         kVertexShader)
          .c_str());
  fragment_shader_ = GLHelper::LoadShader(
      GL_FRAGMENT_SHADER,
      base::StringPrintf("%s\n%s\n%s",
                         use_core_profile ? kFragmentHeaderCoreProfile
                                          : kFragmentHeaderCompatiblityProfile,
                         do_color_conversion.c_str(), kFragmentShader)
          .c_str());
  program_ = GLHelper::SetupProgram(vertex_shader_, fragment_shader_);

  ScopedUseProgram use_program(program_);
  size_location_ = glGetUniformLocation(program_, "a_texScale");
  DCHECK_NE(-1, size_location_);
  int y_sampler_location = glGetUniformLocation(program_, "a_y_texture");
  DCHECK_NE(-1, y_sampler_location);
  int uv_sampler_location = glGetUniformLocation(program_, "a_uv_texture");
  DCHECK_NE(-1, uv_sampler_location);

  glGenTextures(1, &y_texture_);
  glGenTextures(1, &uv_texture_);

  glUniform1i(y_sampler_location, 0);
  glUniform1i(uv_sampler_location, 1);
}

YUVToRGBConverter::~YUVToRGBConverter() {
  glDeleteTextures(1, &y_texture_);
  glDeleteTextures(1, &uv_texture_);
  glDeleteProgram(program_);
  glDeleteShader(vertex_shader_);
  glDeleteShader(fragment_shader_);
  glDeleteBuffersARB(1, &vertex_buffer_);
  glDeleteFramebuffersEXT(1, &framebuffer_);
}

void YUVToRGBConverter::CopyYUV420ToRGB(unsigned target,
                                        const gfx::Size& size,
                                        unsigned rgb_texture) {
  // Note that state restoration is done explicitly instead of scoped binders to
  // avoid https://crbug.com/601729.
  GLint old_active_texture = -1;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_texture);
  GLint old_texture0_binding = -1;
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &old_texture0_binding);
  GLint old_texture1_binding = -1;
  glActiveTexture(GL_TEXTURE1);
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &old_texture1_binding);

  // Allocate the rgb texture.
  glActiveTexture(old_active_texture);
  glBindTexture(target, rgb_texture);
  glTexImage2D(target, 0, GL_RGB, size.width(), size.height(),
               0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

  // Set up and issue the draw call.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, y_texture_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, uv_texture_);
  ScopedFramebufferBinder framebuffer_binder(framebuffer_);
  ScopedViewport viewport(0, 0, size.width(), size.height());
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            target, rgb_texture, 0);
  DCHECK_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatusEXT(GL_FRAMEBUFFER));
  ScopedUseProgram use_program(program_);
  glUniform2f(size_location_, size.width(), size.height());
  GLHelper::DrawQuad(vertex_buffer_);

  // Restore previous state.
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            target, 0, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, old_texture0_binding);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, old_texture1_binding);
  glActiveTexture(old_active_texture);
}

}  // namespace gl
