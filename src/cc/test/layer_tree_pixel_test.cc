// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_test.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "cc/base/switches.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_in_process_context_provider.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_layer_tree_frame_sink.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/ipc/gl_in_process_context.h"

using gpu::gles2::GLES2Interface;

namespace cc {

LayerTreePixelTest::LayerTreePixelTest()
    : pixel_comparator_(new ExactPixelComparator(true)),
      test_type_(PIXEL_TEST_GL),
      property_trees_(nullptr),
      pending_texture_mailbox_callbacks_(0) {}

LayerTreePixelTest::~LayerTreePixelTest() = default;

std::unique_ptr<viz::TestLayerTreeFrameSink>
LayerTreePixelTest::CreateLayerTreeFrameSink(
    const viz::RendererSettings& renderer_settings,
    double refresh_rate,
    scoped_refptr<viz::ContextProvider>,
    scoped_refptr<viz::RasterContextProvider>) {
  scoped_refptr<TestInProcessContextProvider> compositor_context_provider;
  scoped_refptr<TestInProcessContextProvider> worker_context_provider;
  if (test_type_ == PIXEL_TEST_GL) {
    compositor_context_provider = new TestInProcessContextProvider(
        /*enable_oop_rasterization=*/false, /*support_locking=*/false);
    worker_context_provider = new TestInProcessContextProvider(
        /*enable_oop_rasterization=*/false, /*support_locking=*/true);
    // Bind worker context to main thread like it is in production. This is
    // needed to fully initialize the context. Compositor context is bound to
    // the impl thread in LayerTreeFrameSink::BindToCurrentThread().
    gpu::ContextResult result = worker_context_provider->BindToCurrentThread();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  }
  static constexpr bool disable_display_vsync = false;
  bool synchronous_composite =
      !HasImplThread() &&
      !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;
  viz::RendererSettings test_settings = renderer_settings;
  // Keep texture sizes exactly matching the bounds of the RenderPass to avoid
  // floating point badness in texcoords.
  test_settings.dont_round_texture_sizes_for_pixel_tests = true;
  auto delegating_output_surface =
      std::make_unique<viz::TestLayerTreeFrameSink>(
          compositor_context_provider, worker_context_provider,
          gpu_memory_buffer_manager(), test_settings, ImplThreadTaskRunner(),
          synchronous_composite, disable_display_vsync, refresh_rate);
  delegating_output_surface->SetEnlargePassTextureAmount(
      enlarge_texture_amount_);
  return delegating_output_surface;
}

std::unique_ptr<viz::OutputSurface>
LayerTreePixelTest::CreateDisplayOutputSurfaceOnThread(
    scoped_refptr<viz::ContextProvider> compositor_context_provider) {
  std::unique_ptr<PixelTestOutputSurface> display_output_surface;
  if (test_type_ == PIXEL_TEST_GL) {
    // Pixel tests use a separate context for the Display to more closely
    // mimic texture transport from the renderer process to the Display
    // compositor.
    auto display_context_provider =
        base::MakeRefCounted<TestInProcessContextProvider>(
            /*enable_oop_rasterization=*/false, /*support_locking=*/false);
    gpu::ContextResult result = display_context_provider->BindToCurrentThread();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);

    bool flipped_output_surface = false;
    display_output_surface = std::make_unique<PixelTestOutputSurface>(
        std::move(display_context_provider), flipped_output_surface);
  } else {
    display_output_surface = std::make_unique<PixelTestOutputSurface>(
        std::make_unique<viz::SoftwareOutputDevice>());
  }
  return std::move(display_output_surface);
}

std::unique_ptr<viz::CopyOutputRequest>
LayerTreePixelTest::CreateCopyOutputRequest() {
  return std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&LayerTreePixelTest::ReadbackResult,
                     base::Unretained(this)));
}

void LayerTreePixelTest::ReadbackResult(
    std::unique_ptr<viz::CopyOutputResult> result) {
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA_BITMAP);
  result_bitmap_ = std::make_unique<SkBitmap>(result->AsSkBitmap());
  EXPECT_TRUE(result_bitmap_->readyToDraw());
  EndTest();
}

void LayerTreePixelTest::BeginTest() {
  Layer* target =
      readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
  if (!property_trees_) {
    target->RequestCopyOfOutput(CreateCopyOutputRequest());
  } else {
    layer_tree_host()->property_trees()->effect_tree.AddCopyRequest(
        target->effect_tree_index(), CreateCopyOutputRequest());
    layer_tree_host()
        ->property_trees()
        ->effect_tree.Node(target->effect_tree_index())
        ->has_copy_request = true;
  }
  PostSetNeedsCommitToMainThread();
}

void LayerTreePixelTest::AfterTest() {
  // Bitmap comparison.
  if (ref_file_.empty()) {
    EXPECT_TRUE(
        MatchesBitmap(*result_bitmap_, expected_bitmap_, *pixel_comparator_));
    return;
  }

  // File comparison.
  base::FilePath test_data_dir;
  EXPECT_TRUE(
      base::PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir));
  base::FilePath ref_file_path = test_data_dir.Append(ref_file_);

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kCCRebaselinePixeltests))
    EXPECT_TRUE(WritePNGFile(*result_bitmap_, ref_file_path, true));
  EXPECT_TRUE(MatchesPNGFile(*result_bitmap_,
                             ref_file_path,
                             *pixel_comparator_));
}

scoped_refptr<SolidColorLayer> LayerTreePixelTest::CreateSolidColorLayer(
    const gfx::Rect& rect, SkColor color) {
  scoped_refptr<SolidColorLayer> layer = SolidColorLayer::Create();
  layer->SetIsDrawable(true);
  layer->SetBounds(rect.size());
  layer->SetPosition(gfx::PointF(rect.origin()));
  layer->SetOffsetToTransformParent(
      gfx::Vector2dF(rect.origin().x(), rect.origin().y()));
  layer->SetBackgroundColor(color);
  return layer;
}

void LayerTreePixelTest::EndTest() {
  // Drop textures on the main thread so that they can be cleaned up and
  // the pending callbacks will fire.
  for (size_t i = 0; i < texture_layers_.size(); ++i) {
    texture_layers_[i]->ClearTexture();
  }

  TryEndTest();
}

void LayerTreePixelTest::InitializeSettings(LayerTreeSettings* settings) {
  settings->layer_transforms_should_scale_layer_contents = true;
}

void LayerTreePixelTest::TryEndTest() {
  if (!result_bitmap_)
    return;
  if (pending_texture_mailbox_callbacks_)
    return;
  LayerTreeTest::EndTest();
}

scoped_refptr<SolidColorLayer> LayerTreePixelTest::
    CreateSolidColorLayerWithBorder(
        const gfx::Rect& rect, SkColor color,
        int border_width, SkColor border_color) {
  scoped_refptr<SolidColorLayer> layer = CreateSolidColorLayer(rect, color);
  scoped_refptr<SolidColorLayer> border_top = CreateSolidColorLayer(
      gfx::Rect(0, 0, rect.width(), border_width), border_color);
  scoped_refptr<SolidColorLayer> border_left = CreateSolidColorLayer(
      gfx::Rect(0,
                border_width,
                border_width,
                rect.height() - border_width * 2),
      border_color);
  scoped_refptr<SolidColorLayer> border_right =
      CreateSolidColorLayer(gfx::Rect(rect.width() - border_width,
                                      border_width,
                                      border_width,
                                      rect.height() - border_width * 2),
                            border_color);
  scoped_refptr<SolidColorLayer> border_bottom = CreateSolidColorLayer(
      gfx::Rect(0, rect.height() - border_width, rect.width(), border_width),
      border_color);
  layer->AddChild(border_top);
  layer->AddChild(border_left);
  layer->AddChild(border_right);
  layer->AddChild(border_bottom);
  return layer;
}

void LayerTreePixelTest::RunPixelTest(PixelTestType test_type,
                                      scoped_refptr<Layer> content_root,
                                      base::FilePath file_name) {
  test_type_ = test_type;
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunPixelTest(PixelTestType test_type,
                                      scoped_refptr<Layer> content_root,
                                      const SkBitmap& expected_bitmap) {
  test_type_ = test_type;
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = base::FilePath();
  expected_bitmap_ = expected_bitmap;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::RunPixelTestWithLayerList(
    PixelTestType test_type,
    scoped_refptr<Layer> root_layer,
    base::FilePath file_name,
    PropertyTrees* property_trees) {
  test_type_ = test_type;
  content_root_ = root_layer;
  property_trees_ = property_trees;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::InitializeForLayerListMode(
    scoped_refptr<Layer>* root_layer,
    PropertyTrees* property_trees) {
  ClipNode clip_node;
  property_trees->clip_tree.Insert(clip_node, 0);

  EffectNode root_effect;
  root_effect.clip_id = 1;
  root_effect.stable_id = 1;
  root_effect.transform_id = 1;
  root_effect.has_render_surface = true;
  property_trees->effect_tree.Insert(root_effect, 0);

  ScrollNode scroll_node;
  property_trees->scroll_tree.Insert(scroll_node, 0);

  TransformNode transform_node;
  property_trees->transform_tree.Insert(transform_node, 0);

  *root_layer = Layer::Create();
  (*root_layer)->SetBounds(gfx::Size(100, 100));
  (*root_layer)->SetEffectTreeIndex(1);
  (*root_layer)->SetClipTreeIndex(1);
  (*root_layer)->SetScrollTreeIndex(1);
  (*root_layer)->SetTransformTreeIndex(1);
  (*root_layer)
      ->set_property_tree_sequence_number(property_trees->sequence_number);
}

void LayerTreePixelTest::RunSingleThreadedPixelTest(
    PixelTestType test_type,
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  test_type_ = test_type;
  content_root_ = content_root;
  readback_target_ = nullptr;
  ref_file_ = file_name;
  RunTest(CompositorMode::SINGLE_THREADED);
}

void LayerTreePixelTest::RunPixelTestWithReadbackTarget(
    PixelTestType test_type,
    scoped_refptr<Layer> content_root,
    Layer* target,
    base::FilePath file_name) {
  test_type_ = test_type;
  content_root_ = content_root;
  readback_target_ = target;
  ref_file_ = file_name;
  RunTest(CompositorMode::THREADED);
}

void LayerTreePixelTest::SetupTree() {
  if (property_trees_) {
    layer_tree_host()->SetRootLayer(content_root_);
    layer_tree_host()->SetPropertyTreesForTesting(property_trees_);
  } else {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(content_root_->bounds());
    root->AddChild(content_root_);
    layer_tree_host()->SetRootLayer(root);
  }
  LayerTreeTest::SetupTree();
}

SkBitmap LayerTreePixelTest::CopyMailboxToBitmap(
    const gfx::Size& size,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    const gfx::ColorSpace& color_space) {
  SkBitmap bitmap;
  std::unique_ptr<gpu::GLInProcessContext> context =
      CreateTestInProcessContext();
  GLES2Interface* gl = context->GetImplementation();

  if (sync_token.HasData())
    gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  GLuint texture_id = gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);

  GLuint fbo = 0;
  gl->GenFramebuffers(1, &fbo);
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  EXPECT_EQ(static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE),
            gl->CheckFramebufferStatus(GL_FRAMEBUFFER));

  std::unique_ptr<uint8_t[]> pixels(new uint8_t[size.GetArea() * 4]);
  gl->ReadPixels(0,
                 0,
                 size.width(),
                 size.height(),
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels.get());

  gl->DeleteFramebuffers(1, &fbo);
  gl->DeleteTextures(1, &texture_id);

  EXPECT_TRUE(color_space.IsValid());
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size.width(), size.height(),
                                                color_space.ToSkColorSpace()));

  uint8_t* out_pixels = static_cast<uint8_t*>(bitmap.getPixels());

  size_t row_bytes = size.width() * 4;
  size_t total_bytes = size.height() * row_bytes;
  for (size_t dest_y = 0; dest_y < total_bytes; dest_y += row_bytes) {
    // Flip Y axis.
    size_t src_y = total_bytes - dest_y - row_bytes;
    // Swizzle OpenGL -> Skia byte order.
    for (size_t x = 0; x < row_bytes; x += 4) {
      out_pixels[dest_y + x + SK_R32_SHIFT/8] = pixels.get()[src_y + x + 0];
      out_pixels[dest_y + x + SK_G32_SHIFT/8] = pixels.get()[src_y + x + 1];
      out_pixels[dest_y + x + SK_B32_SHIFT/8] = pixels.get()[src_y + x + 2];
      out_pixels[dest_y + x + SK_A32_SHIFT/8] = pixels.get()[src_y + x + 3];
    }
  }

  return bitmap;
}

void LayerTreePixelTest::Finish() {
  std::unique_ptr<gpu::GLInProcessContext> context =
      CreateTestInProcessContext();
  GLES2Interface* gl = context->GetImplementation();
  gl->Finish();
}

}  // namespace cc
