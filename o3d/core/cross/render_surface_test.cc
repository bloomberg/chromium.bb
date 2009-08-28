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


#include "core/cross/precompile.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/client.h"
#include "core/cross/pack.h"
#include "core/cross/renderer.h"
#include "core/cross/bitmap.h"
#include "core/cross/features.h"
#include "core/cross/texture.h"
#include "core/cross/render_surface.h"
#include "core/cross/render_surface_set.h"
#include "core/cross/renderer_platform.h"

// Defined in testing_common.cc, for each platform.
extern o3d::DisplayWindow* g_display_window;

namespace o3d {

class MockRenderer {
 public:
  explicit MockRenderer(Renderer* renderer)
    : renderer_(renderer) {}
  virtual ~MockRenderer() {}

  void StartRendering() {
    renderer_->set_rendering(true);
  }
  void FinishRendering() {
    renderer_->set_rendering(false);
  }
  void SetRenderSurfaces(const RenderSurface* surface,
                         const RenderDepthStencilSurface* depth_surface) {
    renderer_->SetRenderSurfaces(surface, depth_surface);
  }
  void GetRenderSurfaces(const RenderSurface** surface,
                         const RenderDepthStencilSurface** depth_surface) {
    renderer_->GetRenderSurfaces(surface, depth_surface);
  }
 private:
  Renderer* renderer_;
};

class RenderSurfaceTest : public testing::Test {
 public:
  RenderSurfaceTest()
      : object_manager_(g_service_locator) {}

  ServiceLocator* service_locator() {
    return service_locator_;
  }

  MockRenderer* renderer() {
    return renderer_;
  }

 protected:
  virtual void SetUp() {
    service_locator_ = new ServiceLocator;
    features_ = new Features(service_locator_);
    pack_ = object_manager_->CreatePack();
    renderer_ = new MockRenderer(g_renderer);
    renderer_->StartRendering();
  }

  virtual void TearDown() {
    renderer_->FinishRendering();
    pack_->Destroy();
    delete features_;
    delete service_locator_;
    delete renderer_;
  }

  Pack* pack() { return pack_; }

  ServiceDependency<ObjectManager> object_manager_;
  ServiceLocator* service_locator_;
  Features* features_;
  Pack* pack_;
  MockRenderer* renderer_;
};

// Test that non PoT textures can't make render surfaces
TEST_F(RenderSurfaceTest, NonPowerOfTwoRenderSurfaceEnabled) {
  Texture2D* texture = pack()->CreateTexture2D(20, 32, Texture::ARGB8, 2, true);
  ASSERT_TRUE(NULL == texture);
}
// Test that a render surface can be created
TEST_F(RenderSurfaceTest, CreateRenderSurfaceFromTexture2D) {
  Texture2D* texture = pack()->CreateTexture2D(16, 32, Texture::ARGB8, 2, true);
  ASSERT_TRUE(NULL != texture);

  RenderSurface::Ref render_surface = texture->GetRenderSurface(0);
  ASSERT_TRUE(NULL != render_surface);
  ASSERT_TRUE(NULL != render_surface->texture());
  ASSERT_EQ(render_surface->width(), 16);
  ASSERT_EQ(render_surface->height(), 32);
}

TEST_F(RenderSurfaceTest, CreateRenderSurfaceFromTextureCUBE) {
  TextureCUBE* texture = pack()->CreateTextureCUBE(16, Texture::ARGB8, 2, true);
  ASSERT_TRUE(NULL != texture);

  RenderSurface::Ref render_surface = texture->GetRenderSurface(
      TextureCUBE::FACE_POSITIVE_X, 0);
  ASSERT_TRUE(NULL != render_surface);
  ASSERT_TRUE(NULL != render_surface->texture());
  ASSERT_EQ(render_surface->width(), 16);
  ASSERT_EQ(render_surface->height(), 16);
}

TEST_F(RenderSurfaceTest, SwapRenderSurfaces) {
  Texture2D* texture = pack()->CreateTexture2D(16, 32, Texture::ARGB8, 2, true);
  ASSERT_TRUE(NULL != texture);

  RenderSurface::Ref render_surface = texture->GetRenderSurface(0);
  ASSERT_TRUE(NULL != render_surface);
  ASSERT_TRUE(texture == render_surface->texture());

  RenderDepthStencilSurface* depth_surface =
      pack()->CreateDepthStencilSurface(16, 32);

  // Now swap surfaces.
  renderer()->SetRenderSurfaces(render_surface, depth_surface);
  const RenderSurface* test_render_surface = NULL;
  const RenderDepthStencilSurface* test_depth_surface = NULL;
  renderer()->GetRenderSurfaces(&test_render_surface, &test_depth_surface);
  ASSERT_TRUE(test_render_surface == render_surface);
  ASSERT_TRUE(test_depth_surface == depth_surface);
}

TEST_F(RenderSurfaceTest, SetBackSurfaces) {
  Texture2D* texture = pack()->CreateTexture2D(16, 32, Texture::ARGB8, 2, true);
  ASSERT_TRUE(NULL != texture);

  RenderSurface::Ref render_surface = texture->GetRenderSurface(0);
  ASSERT_TRUE(NULL != render_surface);
  ASSERT_TRUE(texture == render_surface->texture());

  RenderDepthStencilSurface* depth_surface =
      pack()->CreateDepthStencilSurface(16, 32);

  // Save the original surfaces for comparison.
  const RenderSurface* original_render_surface = NULL;
  const RenderDepthStencilSurface* original_depth_surface = NULL;
  renderer()->GetRenderSurfaces(&original_render_surface,
                                &original_depth_surface);
  // Now swap surfaces.
  renderer()->SetRenderSurfaces(render_surface, depth_surface);
  // Return the back buffers
  renderer()->SetRenderSurfaces(NULL, NULL);
  // Get the original surfaces again for comparison.
  const RenderSurface* restored_render_surface = NULL;
  const RenderDepthStencilSurface* restored_depth_surface = NULL;
  renderer()->GetRenderSurfaces(&original_render_surface,
                                &original_depth_surface);
  ASSERT_TRUE(original_render_surface == restored_render_surface);
  ASSERT_TRUE(original_depth_surface == restored_depth_surface);
}

TEST_F(RenderSurfaceTest, RenderSurfaceSetTest) {
  Texture2D* texture = pack()->CreateTexture2D(16, 32, Texture::ARGB8, 2, true);
  ASSERT_TRUE(NULL != texture);

  RenderSurface::Ref render_surface = texture->GetRenderSurface(0);
  ASSERT_TRUE(NULL != render_surface);
  ASSERT_TRUE(texture == render_surface->texture());

  RenderDepthStencilSurface* depth_surface =
      pack()->CreateDepthStencilSurface(16, 32);

  RenderSurfaceSet* render_surface_set = pack()->Create<RenderSurfaceSet>();
  render_surface_set->set_render_surface(render_surface);
  render_surface_set->set_render_depth_stencil_surface(depth_surface);
  ASSERT_TRUE(render_surface_set->ValidateBoundSurfaces());

  RenderContext render_context(g_renderer);

  const RenderSurface* old_render_surface = NULL;
  const RenderDepthStencilSurface* old_depth_surface = NULL;
  renderer()->GetRenderSurfaces(&old_render_surface, &old_depth_surface);

  render_surface_set->Render(&render_context);
  const RenderSurface* test_render_surface = NULL;
  const RenderDepthStencilSurface* test_depth_surface = NULL;
  renderer()->GetRenderSurfaces(&test_render_surface, &test_depth_surface);
  ASSERT_TRUE(test_render_surface == render_surface);
  ASSERT_TRUE(test_depth_surface == depth_surface);

  render_surface_set->PostRender(&render_context);
  renderer()->GetRenderSurfaces(&test_render_surface, &test_depth_surface);
  ASSERT_TRUE(test_render_surface == old_render_surface);
  ASSERT_TRUE(test_depth_surface == old_depth_surface);
}

}  // namespace o3d

