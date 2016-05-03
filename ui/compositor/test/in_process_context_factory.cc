// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/surfaces/onscreen_display_client.h"
#include "cc/surfaces/surface_display_output_surface.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/reflector.h"
#include "ui/compositor/test/in_process_context_provider.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ui {
namespace {

class FakeReflector : public Reflector {
 public:
  FakeReflector() {}
  ~FakeReflector() override {}
  void OnMirroringCompositorResized() override {}
  void AddMirroringLayer(Layer* layer) override {}
  void RemoveMirroringLayer(Layer* layer) override {}
};

// An OutputSurface implementation that directly draws and swaps to an actual
// GL surface.
class DirectOutputSurface : public cc::OutputSurface {
 public:
  DirectOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context_provider,
      const scoped_refptr<cc::ContextProvider>& worker_context_provider,
      std::unique_ptr<cc::BeginFrameSource> begin_frame_source)
      : cc::OutputSurface(context_provider, worker_context_provider),
        begin_frame_source_(std::move(begin_frame_source)),
        weak_ptr_factory_(this) {}

  ~DirectOutputSurface() override {}

  // cc::OutputSurface implementation
  bool BindToClient(cc::OutputSurfaceClient* client) override {
    if (!OutputSurface::BindToClient(client))
      return false;

    client->SetBeginFrameSource(begin_frame_source_.get());
    return true;
  }
  void SwapBuffers(cc::CompositorFrame* frame) override {
    DCHECK(context_provider_.get());
    DCHECK(frame->gl_frame_data);
    if (frame->gl_frame_data->sub_buffer_rect ==
        gfx::Rect(frame->gl_frame_data->size)) {
      context_provider_->ContextSupport()->Swap();
    } else {
      context_provider_->ContextSupport()->PartialSwapBuffers(
          frame->gl_frame_data->sub_buffer_rect);
    }
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->ShallowFlushCHROMIUM();

    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

    context_provider_->ContextSupport()->SignalSyncToken(
        sync_token, base::Bind(&OutputSurface::OnSwapBuffersComplete,
                               weak_ptr_factory_.GetWeakPtr()));
    client_->DidSwapBuffers();
  }

 private:
  std::unique_ptr<cc::BeginFrameSource> begin_frame_source_;

  base::WeakPtrFactory<DirectOutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DirectOutputSurface);
};

}  // namespace

InProcessContextFactory::InProcessContextFactory(
    bool context_factory_for_test,
    cc::SurfaceManager* surface_manager)
    : next_surface_id_namespace_(1u),
      use_test_surface_(true),
      context_factory_for_test_(context_factory_for_test),
      surface_manager_(surface_manager) {
  DCHECK_NE(gfx::GetGLImplementation(), gfx::kGLImplementationNone)
      << "If running tests, ensure that main() is calling "
      << "gfx::GLSurfaceTestSupport::InitializeOneOff()";
}

InProcessContextFactory::~InProcessContextFactory() {
  DCHECK(per_compositor_data_.empty());
}

void InProcessContextFactory::CreateOutputSurface(
    base::WeakPtr<Compositor> compositor) {
  // Try to reuse existing shared worker context provider.
  bool shared_worker_context_provider_lost = false;
  if (shared_worker_context_provider_) {
    // Note: If context is lost, delete reference after releasing the lock.
    base::AutoLock lock(*shared_worker_context_provider_->GetLock());
    if (shared_worker_context_provider_->ContextGL()
            ->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      shared_worker_context_provider_lost = true;
    }
  }
  if (!shared_worker_context_provider_ || shared_worker_context_provider_lost) {
    shared_worker_context_provider_ = InProcessContextProvider::CreateOffscreen(
        &gpu_memory_buffer_manager_, &image_factory_, nullptr);
    if (shared_worker_context_provider_ &&
        !shared_worker_context_provider_->BindToCurrentThread())
      shared_worker_context_provider_ = nullptr;
  }

  gpu::gles2::ContextCreationAttribHelper attribs;
  attribs.alpha_size = 8;
  attribs.blue_size = 8;
  attribs.green_size = 8;
  attribs.red_size = 8;
  attribs.depth_size = 0;
  attribs.stencil_size = 0;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;
  scoped_refptr<InProcessContextProvider> context_provider =
      InProcessContextProvider::Create(
          attribs, shared_worker_context_provider_.get(),
          &gpu_memory_buffer_manager_, &image_factory_, compositor->widget(),
          "UICompositor");

  std::unique_ptr<cc::OutputSurface> real_output_surface;
  std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source(
      new cc::SyntheticBeginFrameSource(compositor->task_runner().get(),
                                        cc::BeginFrameArgs::DefaultInterval()));

  if (use_test_surface_) {
    bool flipped_output_surface = false;
    real_output_surface = base::WrapUnique(new cc::PixelTestOutputSurface(
        context_provider, shared_worker_context_provider_,
        flipped_output_surface, std::move(begin_frame_source)));
  } else {
    real_output_surface = base::WrapUnique(new DirectOutputSurface(
        context_provider, shared_worker_context_provider_,
        std::move(begin_frame_source)));
  }

  if (surface_manager_) {
    std::unique_ptr<cc::OnscreenDisplayClient> display_client(
        new cc::OnscreenDisplayClient(
            std::move(real_output_surface), surface_manager_,
            GetSharedBitmapManager(), GetGpuMemoryBufferManager(),
            compositor->GetRendererSettings(), compositor->task_runner(),
            compositor->surface_id_allocator()->id_namespace()));
    std::unique_ptr<cc::SurfaceDisplayOutputSurface> surface_output_surface(
        new cc::SurfaceDisplayOutputSurface(
            surface_manager_, compositor->surface_id_allocator(),
            context_provider, shared_worker_context_provider_));
    display_client->set_surface_output_surface(surface_output_surface.get());
    surface_output_surface->set_display_client(display_client.get());

    compositor->SetOutputSurface(std::move(surface_output_surface));

    delete per_compositor_data_[compositor.get()];
    per_compositor_data_[compositor.get()] = display_client.release();
  } else {
    compositor->SetOutputSurface(std::move(real_output_surface));
  }
}

std::unique_ptr<Reflector> InProcessContextFactory::CreateReflector(
    Compositor* mirrored_compositor,
    Layer* mirroring_layer) {
  return base::WrapUnique(new FakeReflector);
}

void InProcessContextFactory::RemoveReflector(Reflector* reflector) {
}

scoped_refptr<cc::ContextProvider>
InProcessContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return shared_main_thread_contexts_;

  shared_main_thread_contexts_ = InProcessContextProvider::CreateOffscreen(
      &gpu_memory_buffer_manager_, &image_factory_, nullptr);
  if (shared_main_thread_contexts_.get() &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void InProcessContextFactory::RemoveCompositor(Compositor* compositor) {
  if (!per_compositor_data_.count(compositor))
    return;
  delete per_compositor_data_[compositor];
  per_compositor_data_.erase(compositor);
}

bool InProcessContextFactory::DoesCreateTestContexts() {
  return context_factory_for_test_;
}

uint32_t InProcessContextFactory::GetImageTextureTarget(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return GL_TEXTURE_2D;
}

cc::SharedBitmapManager* InProcessContextFactory::GetSharedBitmapManager() {
  return &shared_bitmap_manager_;
}

gpu::GpuMemoryBufferManager*
InProcessContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* InProcessContextFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

std::unique_ptr<cc::SurfaceIdAllocator>
InProcessContextFactory::CreateSurfaceIdAllocator() {
  std::unique_ptr<cc::SurfaceIdAllocator> allocator(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
  if (surface_manager_)
    allocator->RegisterSurfaceIdNamespace(surface_manager_);
  return allocator;
}

void InProcessContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                            const gfx::Size& size) {
  if (!per_compositor_data_.count(compositor))
    return;
  per_compositor_data_[compositor]->display()->Resize(size);
}

}  // namespace ui
