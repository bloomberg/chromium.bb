// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_native_pixmap.h"

#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {

namespace {

class GLImageNativePixmapTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    std::vector<GLImplementation> allowed_impls =
        init::GetAllowedGLImplementations();

    GLImplementation gl_impl = kGLImplementationEGLGLES2;
    bool found = false;
    for (auto impl : allowed_impls) {
      if (impl == gl_impl) {
        found = true;
        break;
      }
    }

    if (!found) {
      LOG(WARNING) << "Skip test, egl is required";
      return;
    }

    GLSurfaceTestSupport::InitializeOneOffImplementation(
        gl_impl, /* fallback_to_osmesa */ false);

    const std::string dmabuf_import_ext = "EGL_MESA_image_dma_buf_export";
    std::string platform_extensions(DriverEGL::GetPlatformExtensions());
    ExtensionSet extensions(MakeExtensionSet(platform_extensions));
    if (!HasExtension(extensions, dmabuf_import_ext)) {
      LOG(WARNING) << "Skip test, missing extension " << dmabuf_import_ext;
      return;
    }

    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    context_ =
        gl::init::CreateGLContext(nullptr, surface_.get(), GLContextAttribs());
    context_->MakeCurrent(surface_.get());

    skip_test_ = false;
  }

  void TearDown() override {
    if (context_)
      context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
    surface_ = nullptr;
    init::ShutdownGL(false);
  }

  bool skip_test_ = true;
  scoped_refptr<GLSurface> surface_;
  scoped_refptr<GLContext> context_;
};

void GLTexture2DToDmabuf(gfx::BufferFormat image_format,
                         GLint tex_internal_format,
                         GLenum tex_format) {
  const gfx::Size image_size(64, 64);

  EXPECT_NE(nullptr, GLContext::GetCurrent());

  EGLContext context =
      reinterpret_cast<EGLContext>(GLContext::GetCurrent()->GetHandle());
  EXPECT_NE(EGL_NO_CONTEXT, context);

  scoped_refptr<gl::GLImageNativePixmap> image(new gl::GLImageNativePixmap(
      image_size,
      gl::GLImageNativePixmap::GetInternalFormatForTesting(image_format)));

  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  EXPECT_NE(0u, texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, tex_internal_format, image_size.width(),
               image_size.height(), 0, tex_format, GL_UNSIGNED_BYTE, nullptr);

  scoped_refptr<gl::GLImageEGL> base_image = image;
  EXPECT_TRUE(base_image->Initialize(
      context, EGL_GL_TEXTURE_2D_KHR,
      reinterpret_cast<EGLClientBuffer>(texture_id), nullptr));

  gfx::NativePixmapHandle native_pixmap_handle = image->ExportHandle();

  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(image_format);
  EXPECT_EQ(num_planes, native_pixmap_handle.planes.size());

  std::vector<base::ScopedFD> scoped_fds;
  for (auto& fd : native_pixmap_handle.fds) {
    EXPECT_TRUE(fd.auto_close);
    scoped_fds.emplace_back(fd.fd);
    EXPECT_TRUE(scoped_fds.back().is_valid());
  }

  // Close all fds.
  scoped_fds.clear();

  image = nullptr;
  glDeleteTextures(1, &texture_id);
}

TEST_F(GLImageNativePixmapTest, GLTexture2DToDmabuf) {
  if (skip_test_)
    return;

  // Add more cases if needed.
  GLTexture2DToDmabuf(gfx::BufferFormat::RGBA_8888, GL_RGBA, GL_RGBA);
  GLTexture2DToDmabuf(gfx::BufferFormat::BGRA_8888, GL_RGBA, GL_RGBA);
  GLTexture2DToDmabuf(gfx::BufferFormat::RGBX_8888, GL_RGBA, GL_RGBA);
  GLTexture2DToDmabuf(gfx::BufferFormat::BGRX_8888, GL_RGBA, GL_RGBA);
}

}  // namespace

}  // namespace gl
