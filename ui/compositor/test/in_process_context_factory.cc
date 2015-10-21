// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_factory.h"

#include "base/bind.h"
#include "base/command_line.h"
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
      const scoped_refptr<cc::ContextProvider>& worker_context_provider)
      : cc::OutputSurface(context_provider, worker_context_provider),
        weak_ptr_factory_(this) {}

  ~DirectOutputSurface() override {}

  // cc::OutputSurface implementation
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
    uint32_t sync_point =
        context_provider_->ContextGL()->InsertSyncPointCHROMIUM();
    context_provider_->ContextSupport()->SignalSyncPoint(
        sync_point, base::Bind(&OutputSurface::OnSwapBuffersComplete,
                               weak_ptr_factory_.GetWeakPtr()));
    client_->DidSwapBuffers();
  }

 private:
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

  Layer::InitializeUILayerSettings();
}

InProcessContextFactory::~InProcessContextFactory() {
  DCHECK(per_compositor_data_.empty());
}

void InProcessContextFactory::CreateOutputSurface(
    base::WeakPtr<Compositor> compositor) {
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
      InProcessContextProvider::Create(attribs, &gpu_memory_buffer_manager_,
                                       &image_factory_,
                                       compositor->widget(), "UICompositor");

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
        &gpu_memory_buffer_manager_, &image_factory_);
    if (shared_worker_context_provider_ &&
        !shared_worker_context_provider_->BindToCurrentThread())
      shared_worker_context_provider_ = nullptr;
    if (shared_worker_context_provider_)
      shared_worker_context_provider_->SetupLock();
  }

  scoped_ptr<cc::OutputSurface> real_output_surface;

  if (use_test_surface_) {
    bool flipped_output_surface = false;
    real_output_surface = make_scoped_ptr(new cc::PixelTestOutputSurface(
        context_provider, shared_worker_context_provider_,
        flipped_output_surface));
  } else {
    real_output_surface = make_scoped_ptr(new DirectOutputSurface(
        context_provider, shared_worker_context_provider_));
  }

  if (surface_manager_) {
    scoped_ptr<cc::OnscreenDisplayClient> display_client(
        new cc::OnscreenDisplayClient(
            real_output_surface.Pass(), surface_manager_,
            GetSharedBitmapManager(), GetGpuMemoryBufferManager(),
            compositor->GetRendererSettings(), compositor->task_runner()));
    scoped_ptr<cc::SurfaceDisplayOutputSurface> surface_output_surface(
        new cc::SurfaceDisplayOutputSurface(
            surface_manager_, compositor->surface_id_allocator(),
            context_provider, shared_worker_context_provider_));
    display_client->set_surface_output_surface(surface_output_surface.get());
    surface_output_surface->set_display_client(display_client.get());

    compositor->SetOutputSurface(surface_output_surface.Pass());

    delete per_compositor_data_[compositor.get()];
    per_compositor_data_[compositor.get()] = display_client.release();
  } else {
    compositor->SetOutputSurface(real_output_surface.Pass());
  }
}

scoped_ptr<Reflector> InProcessContextFactory::CreateReflector(
    Compositor* mirrored_compositor,
    Layer* mirroring_layer) {
  return make_scoped_ptr(new FakeReflector);
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
      &gpu_memory_buffer_manager_, &image_factory_);
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

uint32 InProcessContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
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

scoped_ptr<cc::SurfaceIdAllocator>
InProcessContextFactory::CreateSurfaceIdAllocator() {
  scoped_ptr<cc::SurfaceIdAllocator> allocator(
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
