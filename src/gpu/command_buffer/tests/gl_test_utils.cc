// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_test_utils.h"

#include <GLES2/gl2extchromium.h>
#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_LINUX)
#include "ui/gl/gl_image_native_pixmap.h"
#endif

namespace gpu {

// GCC requires these declarations, but MSVC requires they not be present.
#ifndef COMPILER_MSVC
const uint8_t GLTestHelper::kCheckClearValue;
#endif

bool GLTestHelper::InitializeGL(gl::GLImplementation gl_impl) {
  if (gl_impl == gl::GLImplementation::kGLImplementationNone) {
    if (!gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ true))
      return false;
  } else {
    if (!gl::init::InitializeStaticGLBindingsImplementation(
            gl_impl, /*fallback_to_software_gl*/ false))
      return false;

    if (!gl::init::InitializeGLOneOffPlatformImplementation(
            false,  // fallback_to_software_gl
            false,  // disable_gl_drawing
            false   // init_extensions
            )) {
      return false;
    }
  }

  gpu::GPUInfo gpu_info;
  gpu::CollectGraphicsInfoForTesting(&gpu_info);
  gpu::GLManager::g_gpu_feature_info = gpu::ComputeGpuFeatureInfo(
      gpu_info, gpu::GpuPreferences(), base::CommandLine::ForCurrentProcess(),
      nullptr  // needs_more_info
      );

  gl::init::SetDisabledExtensionsPlatform(
      gpu::GLManager::g_gpu_feature_info.disabled_extensions);
  return gl::init::InitializeExtensionSettingsOneOffPlatform();
}

bool GLTestHelper::InitializeGLDefault() {
  return GLTestHelper::InitializeGL(
      gl::GLImplementation::kGLImplementationNone);
}

bool GLTestHelper::HasExtension(const char* extension) {
  // Pad with an extra space to ensure that |extension| is not a substring of
  // another extension.
  std::string extensions =
      std::string(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))) +
      " ";
  std::string extension_padded = std::string(extension) + " ";
  return extensions.find(extension_padded) != std::string::npos;
}

bool GLTestHelper::CheckGLError(const char* msg, int line) {
  bool success = true;
  GLenum error = GL_NO_ERROR;
  while ((error = glGetError()) != GL_NO_ERROR) {
    success = false;
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), error)
        << "GL ERROR in " << msg << " at line " << line << " : " << error;
  }
  return success;
}

GLuint GLTestHelper::CompileShader(GLenum type, const char* shaderSrc) {
  GLuint shader = glCreateShader(type);
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, nullptr);
  // Compile the shader
  glCompileShader(shader);

  return shader;
}

GLuint GLTestHelper::LoadShader(GLenum type, const char* shaderSrc) {
  GLuint shader = CompileShader(type, shaderSrc);

  // Check the compile status
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    EXPECT_EQ(1, value) << "Error compiling shader: " << log;
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

GLuint GLTestHelper::LinkProgram(
    GLuint vertex_shader, GLuint fragment_shader) {
  // Create the program object
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program
  glLinkProgram(program);

  return program;
}

GLuint GLTestHelper::SetupProgram(
    GLuint vertex_shader, GLuint fragment_shader) {
  GLuint program = LinkProgram(vertex_shader, fragment_shader);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    EXPECT_EQ(1, linked) << "Error linking program: " << log;
    glDeleteProgram(program);
    program = 0;
  }
  return program;
}

GLuint GLTestHelper::LoadProgram(
    const char* vertex_shader_source,
    const char* fragment_shader_source) {
  GLuint vertex_shader = LoadShader(
      GL_VERTEX_SHADER, vertex_shader_source);
  GLuint fragment_shader = LoadShader(
      GL_FRAGMENT_SHADER, fragment_shader_source);
  if (!vertex_shader || !fragment_shader) {
    return 0;
  }
  return SetupProgram(vertex_shader, fragment_shader);
}

GLuint GLTestHelper::SetupUnitQuad(GLint position_location) {
  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  static const float vertices[] = {
      1.0f, 1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
  };
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(position_location);
  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 0, 0);

  return vbo;
}

std::vector<GLuint> GLTestHelper::SetupIndexedUnitQuad(
    GLint position_location) {
  GLuint array_buffer = SetupUnitQuad(position_location);
  static const uint8_t indices[] = {0, 1, 2, 3, 4, 5};
  GLuint index_buffer = 0;
  glGenBuffers(1, &index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6, indices, GL_STATIC_DRAW);
  std::vector<GLuint> buffers(2);
  buffers[0] = array_buffer;
  buffers[1] = index_buffer;
  return buffers;
}

GLuint GLTestHelper::SetupColorsForUnitQuad(
    GLint location, const GLfloat color[4], GLenum usage) {
  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLfloat vertices[6 * 4];
  for (int ii = 0; ii < 6; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      vertices[ii * 4 + jj] = color[jj];
    }
  }
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, usage);
  glEnableVertexAttribArray(location);
  glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0, 0);

  return vbo;
}

bool GLTestHelper::CheckPixels(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLint tolerance,
                               const uint8_t* color,
                               const uint8_t* mask) {
  std::vector<uint8_t> colors(width * height * 4);
  for (int i = 0; i < width * height * 4; i += 4)
    memcpy(&colors[i], color, 4);
  return CheckPixels(x, y, width, height, tolerance, colors, mask);
}

bool GLTestHelper::CheckPixels(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLint tolerance,
                               const std::vector<uint8_t>& expected,
                               const uint8_t* mask) {
  GLsizei size = width * height * 4;
  std::vector<uint8_t> pixels(size, kCheckClearValue);
  glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  int bad_count = 0;
  for (GLint yy = 0; yy < height; ++yy) {
    for (GLint xx = 0; xx < width; ++xx) {
      int offset = yy * width * 4 + xx * 4;
      for (int jj = 0; jj < 4; ++jj) {
        uint8_t actual = pixels[offset + jj];
        uint8_t expected_component = expected[offset + jj];
        int diff = actual - expected_component;
        diff = diff < 0 ? -diff: diff;
        if ((!mask || mask[jj]) && diff > tolerance) {
          EXPECT_EQ(static_cast<int>(expected_component),
                    static_cast<int>(actual))
              << " at " << (xx + x) << ", " << (yy + y) << " channel " << jj;
          ++bad_count;
          // Exit early just so we don't spam the log but we print enough
          // to hopefully make it easy to diagnose the issue.
          if (bad_count > 16) {
            return false;
          }
        }
      }
    }
  }
  return bad_count == 0;
}

namespace {

void Set16BitValue(uint8_t dest[2], uint16_t value) {
  dest[0] = value & 0xFFu;
  dest[1] = value >> 8;
}

void Set32BitValue(uint8_t dest[4], uint32_t value) {
  dest[0] = (value >> 0) & 0xFFu;
  dest[1] = (value >> 8) & 0xFFu;
  dest[2] = (value >> 16) & 0xFFu;
  dest[3] = (value >> 24) & 0xFFu;
}

struct BitmapHeaderFile {
  uint8_t magic[2];
  uint8_t size[4];
  uint8_t reserved[4];
  uint8_t offset[4];
};

struct BitmapInfoHeader{
  uint8_t size[4];
  uint8_t width[4];
  uint8_t height[4];
  uint8_t planes[2];
  uint8_t bit_count[2];
  uint8_t compression[4];
  uint8_t size_image[4];
  uint8_t x_pels_per_meter[4];
  uint8_t y_pels_per_meter[4];
  uint8_t clr_used[4];
  uint8_t clr_important[4];
};

}  // namespace

bool GLTestHelper::SaveBackbufferAsBMP(
    const char* filename, int width, int height) {
  FILE* fp = fopen(filename, "wb");
  EXPECT_TRUE(fp != nullptr);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  int num_pixels = width * height;
  int size = num_pixels * 4;
  std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
  uint8_t* pixels = data.get();
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // RGBA to BGRA
  for (int ii = 0; ii < num_pixels; ++ii) {
    int offset = ii * 4;
    uint8_t t = pixels[offset + 0];
    pixels[offset + 0] = pixels[offset + 2];
    pixels[offset + 2] = t;
  }

  BitmapHeaderFile bhf;
  BitmapInfoHeader bih;

  bhf.magic[0] = 'B';
  bhf.magic[1] = 'M';
  Set32BitValue(bhf.size, 0);
  Set32BitValue(bhf.reserved, 0);
  Set32BitValue(bhf.offset, sizeof(bhf) + sizeof(bih));

  Set32BitValue(bih.size, sizeof(bih));
  Set32BitValue(bih.width, width);
  Set32BitValue(bih.height, height);
  Set16BitValue(bih.planes, 1);
  Set16BitValue(bih.bit_count, 32);
  Set32BitValue(bih.compression, 0);
  Set32BitValue(bih.x_pels_per_meter, 0);
  Set32BitValue(bih.y_pels_per_meter, 0);
  Set32BitValue(bih.clr_used, 0);
  Set32BitValue(bih.clr_important, 0);

  fwrite(&bhf, sizeof(bhf), 1, fp);
  fwrite(&bih, sizeof(bih), 1, fp);
  fwrite(pixels, size, 1, fp);
  fclose(fp);
  return true;
}

void GLTestHelper::DrawTextureQuad(const GLenum texture_target,
                                   const char* vertex_src,
                                   const char* fragment_src,
                                   const char* position_name,
                                   const char* sampler_name,
                                   const char* face_name) {
  GLuint program = GLTestHelper::LoadProgram(vertex_src, fragment_src);
  EXPECT_NE(program, 0u);
  glUseProgram(program);

  GLint position_loc = glGetAttribLocation(program, position_name);
  GLint sampler_location = glGetUniformLocation(program, sampler_name);
  ASSERT_NE(position_loc, -1);
  ASSERT_NE(sampler_location, -1);
  GLint face_loc = -1;
  if (gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(texture_target) ==
      GL_TEXTURE_CUBE_MAP) {
    ASSERT_NE(face_name, nullptr);
    face_loc = glGetUniformLocation(program, face_name);
    ASSERT_NE(face_loc, -1);
    glUniform1i(face_loc, texture_target);
  }

  GLuint vertex_buffer = GLTestHelper::SetupUnitQuad(position_loc);
  ASSERT_NE(vertex_buffer, 0u);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(sampler_location, 0);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glDeleteProgram(program);
  glDeleteBuffers(1, &vertex_buffer);
}

GpuCommandBufferTestEGL::GpuCommandBufferTestEGL() : gl_reinitialized_(false) {}

GpuCommandBufferTestEGL::~GpuCommandBufferTestEGL() {}

bool GpuCommandBufferTestEGL::InitializeEGLGLES2(int width, int height) {
  if (gl::GetGLImplementation() !=
      gl::GLImplementation::kGLImplementationEGLGLES2) {
    const auto impls = gl::init::GetAllowedGLImplementations();
    if (!base::Contains(impls,
                        gl::GLImplementation::kGLImplementationEGLGLES2)) {
      LOG(INFO) << "Skip test, implementation EGLGLES2 is not available";
      return false;
    }

    gpu::GPUInfo gpu_info;
    gpu::CollectContextGraphicsInfo(&gpu_info);
    // See crbug.com/822716, the ATI proprietary driver has eglGetProcAddress
    // but eglInitialize crashes with x11.
    if (gpu_info.gl_vendor.find("ATI Technologies Inc.") != std::string::npos) {
      LOG(INFO) << "Skip test, ATI proprietary driver crashes with egl/x11";
      return false;
    }

    gl_reinitialized_ = true;
    gl::init::ShutdownGL(false /* due_to_fallback */);
    if (!GLTestHelper::InitializeGL(
            gl::GLImplementation::kGLImplementationEGLGLES2)) {
      LOG(INFO) << "Skip test, failed to initialize EGLGLES2";
      return false;
    }
  }

  DCHECK_EQ(gl::GLImplementation::kGLImplementationEGLGLES2,
            gl::GetGLImplementation());

  // Make the GL context current now to get all extensions.
  GLManager::Options options;
  options.size = gfx::Size(width, height);
  gl_.Initialize(options);
  gl_.MakeCurrent();

  gl_extensions_ =
      gfx::MakeExtensionSet(gl::GetGLExtensionsFromCurrentContext());
  gl::GLVersionInfo gl_version_info(
      reinterpret_cast<const char*>(glGetString(GL_VERSION)),
      reinterpret_cast<const char*>(glGetString(GL_RENDERER)), gl_extensions_);
  bool result = gl::init::GetGLWindowSystemBindingInfo(
      gl_version_info, &window_system_binding_info_);
  DCHECK(result);

  egl_extensions_ =
      gfx::MakeExtensionSet(window_system_binding_info_.extensions);

  return true;
}

void GpuCommandBufferTestEGL::RestoreGLDefault() {
  gl_.Destroy();

  if (gl_reinitialized_) {
    gl::init::ShutdownGL(false /* due_to_fallback */);
    GLTestHelper::InitializeGLDefault();
  }

  gl_reinitialized_ = false;
  gl_extensions_.clear();
  egl_extensions_.clear();
  window_system_binding_info_ = gl::GLWindowSystemBindingInfo();
}

#if defined(OS_LINUX)
scoped_refptr<gl::GLImageNativePixmap>
GpuCommandBufferTestEGL::CreateGLImageNativePixmap(gfx::BufferFormat format,
                                                   gfx::Size size,
                                                   uint8_t* pixels) const {
  // Upload raw pixels to a new GL texture.
  GLuint tex_client_id = 0;
  glGenTextures(1, &tex_client_id);
  DCHECK_NE(0u, tex_client_id);
  glBindTexture(GL_TEXTURE_2D, tex_client_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // Make sure the texture exists in the service side.
  glFinish();

  // This works because the test run in a similar mode as In-Process-GPU.
  unsigned int tex_service_id = 0;
  gl_.decoder()->GetServiceTextureId(tex_client_id, &tex_service_id);
  EXPECT_NE(0u, tex_service_id);

  // Create an EGLImage from the real texture id.
  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size, format);
  bool result = image->InitializeFromTexture(tex_service_id);
  DCHECK(result);

  // The test will own the EGLImage no need to keep a reference on the GL
  // texture after returning from this function. This is covered by the
  // EGL_KHR_image_base.txt specification, i.e. the underlying memory remains
  // allocated as long as there is at least one sibling (like ref count).
  glDeleteTextures(1, &tex_client_id);

  return image;
}

gfx::NativePixmapHandle GpuCommandBufferTestEGL::CreateNativePixmapHandle(
    gfx::BufferFormat format,
    gfx::Size size,
    uint8_t* pixels) {
  scoped_refptr<gl::GLImageNativePixmap> image =
      CreateGLImageNativePixmap(format, size, pixels);
  EXPECT_TRUE(image);
  EXPECT_EQ(size, image->GetSize());

  // Export the EGLImage as dmabuf fds
  // The test will own the dmabuf fds so no need to keep a reference on the
  // EGLImage after returning from this function.
  return image->ExportHandle();
}
#endif

}  // namespace gpu
