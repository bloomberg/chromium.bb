// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_MOCK_H_
#define UI_GL_GL_IMAGE_MOCK_H_

#include "base/basictypes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GLImageMock : public GLImage {
 public:
  GLImageMock(gfx::GpuMemoryBufferHandle handle, gfx::Size size);

  MOCK_METHOD0(BindTexImage, bool());
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD0(GetSize, gfx::Size());
  MOCK_METHOD0(ReleaseTexImage, void());
 private:
  virtual ~GLImageMock();
  DISALLOW_COPY_AND_ASSIGN(GLImageMock);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_MOCK_H_
