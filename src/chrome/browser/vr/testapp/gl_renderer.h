// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TESTAPP_GL_RENDERER_H_
#define CHROME_BROWSER_VR_TESTAPP_GL_RENDERER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/base_graphics_delegate.h"
#include "ui/gfx/swap_result.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace vr {

class VrTestContext;

// This class manages an OpenGL context and initiates per-frame rendering.
class GlRenderer : public BaseGraphicsDelegate {
 public:
  GlRenderer();
  ~GlRenderer() override;

  // GraphicsDelegate implementation.
  bool Initialize(const scoped_refptr<gl::GLSurface>& surface) override;
  FovRectangles GetRecommendedFovs() override;
  float GetZNear() override;
  RenderInfo GetRenderInfo(FrameType frame_type,
                           const gfx::Transform& head_pose) override;
  RenderInfo GetOptimizedRenderInfoForFovs(const FovRectangles& fovs) override;
  void InitializeBuffers() override;
  void PrepareBufferForWebXr() override;
  void PrepareBufferForWebXrOverlayElements() override;
  void PrepareBufferForContentQuadLayer(
      const gfx::Transform& quad_transform) override;
  void PrepareBufferForBrowserUi() override;
  void OnFinishedDrawingBuffer() override;
  void GetWebXrDrawParams(int* texture_id, Transform* uv_transform) override;
  bool IsContentQuadReady() override;
  void ResumeContentRendering() override;
  void BufferBoundsChanged(const gfx::Size& content_buffer_size,
                           const gfx::Size& overlay_buffer_size) override;
  void GetContentQuadDrawParams(Transform* uv_transform,
                                float* border_x,
                                float* border_y) override;
  int GetContentBufferWidth() override;

  void SetFrameDumpFilepathBase(std::string& filepath_base) override;

  void RenderFrame();
  void PostRenderFrameTask();
  void set_vr_context(VrTestContext* vr_context) { vr_context_ = vr_context; }

 private:
  VrTestContext* vr_context_;

  base::WeakPtrFactory<GlRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GlRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TESTAPP_GL_RENDERER_H_
