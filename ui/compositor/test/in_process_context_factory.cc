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
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/reflector.h"
#include "ui/compositor/test/in_process_context_provider.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

namespace ui {
namespace {

// An OutputSurface implementation that directly draws and swaps to an actual
// GL surface.
class DirectOutputSurface : public cc::OutputSurface {
 public:
  explicit DirectOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context_provider)
      : cc::OutputSurface(context_provider), weak_ptr_factory_(this) {}

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

InProcessContextFactory::InProcessContextFactory(bool context_factory_for_test)
    : next_surface_id_namespace_(1u),
      use_test_surface_(true),
      context_factory_for_test_(context_factory_for_test) {
  DCHECK_NE(gfx::GetGLImplementation(), gfx::kGLImplementationNone)
      << "If running tests, ensure that main() is calling "
      << "gfx::GLSurface::InitializeOneOffForTests()";
}

InProcessContextFactory::~InProcessContextFactory() {}

void InProcessContextFactory::CreateOutputSurface(
    base::WeakPtr<Compositor> compositor,
    bool software_fallback) {
  DCHECK(!software_fallback);
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
  bool lose_context_when_out_of_memory = true;

  scoped_refptr<InProcessContextProvider> context_provider =
      InProcessContextProvider::Create(attribs, lose_context_when_out_of_memory,
                                       compositor->widget(), "UICompositor");

  if (use_test_surface_) {
    bool flipped_output_surface = false;
    compositor->SetOutputSurface(make_scoped_ptr(new cc::PixelTestOutputSurface(
        context_provider, flipped_output_surface)));
  } else {
    compositor->SetOutputSurface(
        make_scoped_ptr(new DirectOutputSurface(context_provider)));
  }
}

scoped_refptr<Reflector> InProcessContextFactory::CreateReflector(
    Compositor* mirroed_compositor,
    Layer* mirroring_layer) {
  return new Reflector();
}

void InProcessContextFactory::RemoveReflector(
    scoped_refptr<Reflector> reflector) {}

scoped_refptr<cc::ContextProvider>
InProcessContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_.get() &&
      !shared_main_thread_contexts_->DestroyedOnMainThread())
    return shared_main_thread_contexts_;

  bool lose_context_when_out_of_memory = false;
  shared_main_thread_contexts_ = InProcessContextProvider::CreateOffscreen(
      lose_context_when_out_of_memory);
  if (shared_main_thread_contexts_.get() &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void InProcessContextFactory::RemoveCompositor(Compositor* compositor) {}

bool InProcessContextFactory::DoesCreateTestContexts() {
  return context_factory_for_test_;
}

cc::SharedBitmapManager* InProcessContextFactory::GetSharedBitmapManager() {
  return &shared_bitmap_manager_;
}

gpu::GpuMemoryBufferManager*
InProcessContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

base::MessageLoopProxy* InProcessContextFactory::GetCompositorMessageLoop() {
  if (!compositor_thread_)
    return NULL;
  return compositor_thread_->message_loop_proxy().get();
}

scoped_ptr<cc::SurfaceIdAllocator>
InProcessContextFactory::CreateSurfaceIdAllocator() {
  return make_scoped_ptr(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
}

void InProcessContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                            const gfx::Size& size) {
}

}  // namespace ui
