// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_STUB_H_
#define UI_GL_GL_CONTEXT_STUB_H_

#include "base/macros.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GLShareGroup;

// A GLContext that does nothing for unit tests.
class GL_EXPORT GLContextStub : public GLContextReal {
 public:
  GLContextStub();
  explicit GLContextStub(GLShareGroup* share_group);

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  const GLContextAttribs& attribs) override;
  bool MakeCurrent(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override;
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override;
  std::string GetGLVersion() override;
  std::string GetGLRenderer() override;
  bool WasAllocatedUsingRobustnessExtension() override;

  void SetUseStubApi(bool stub_api);
  void SetExtensionsString(const char* extensions);
  void SetGLVersionString(const char* version_str);

 protected:
  ~GLContextStub() override;

  GLApi* CreateGLApi(DriverGL* driver) override;

 private:
  bool use_stub_api_;
  std::string version_str_;

  DISALLOW_COPY_AND_ASSIGN(GLContextStub);
};

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_STUB_H_
