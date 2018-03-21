// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_provider.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace ui {

// static
scoped_refptr<InProcessContextProvider> InProcessContextProvider::Create(
    const gpu::ContextCreationAttribs& attribs,
    InProcessContextProvider* shared_context,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    gpu::SurfaceHandle window,
    const std::string& debug_name,
    bool support_locking) {
  return new InProcessContextProvider(attribs, shared_context,
                                      gpu_memory_buffer_manager, image_factory,
                                      window, debug_name, support_locking);
}

// static
scoped_refptr<InProcessContextProvider>
InProcessContextProvider::CreateOffscreen(
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    InProcessContextProvider* shared_context,
    bool support_locking) {
  gpu::ContextCreationAttribs attribs;
  attribs.alpha_size = 8;
  attribs.blue_size = 8;
  attribs.green_size = 8;
  attribs.red_size = 8;
  attribs.depth_size = 0;
  attribs.stencil_size = 8;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;
  return new InProcessContextProvider(
      attribs, shared_context, gpu_memory_buffer_manager, image_factory,
      gpu::kNullSurfaceHandle, "Offscreen", support_locking);
}

InProcessContextProvider::InProcessContextProvider(
    const gpu::ContextCreationAttribs& attribs,
    InProcessContextProvider* shared_context,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    gpu::SurfaceHandle window,
    const std::string& debug_name,
    bool support_locking)
    : support_locking_(support_locking),
      attribs_(attribs),
      shared_context_(shared_context),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      image_factory_(image_factory),
      window_(window),
      debug_name_(debug_name) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  context_thread_checker_.DetachFromThread();
}

InProcessContextProvider::~InProcessContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

void InProcessContextProvider::AddRef() const {
  base::RefCountedThreadSafe<InProcessContextProvider>::AddRef();
}

void InProcessContextProvider::Release() const {
  base::RefCountedThreadSafe<InProcessContextProvider>::Release();
}

gpu::ContextResult InProcessContextProvider::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (bind_tried_)
    return bind_result_;
  bind_tried_ = true;

  context_ = gpu::GLInProcessContext::CreateWithoutInit();
  bind_result_ = context_->Initialize(
      nullptr,  /* service */
      nullptr,  /* surface */
      !window_, /* is_offscreen */
      window_, (shared_context_ ? shared_context_->context_.get() : nullptr),
      attribs_, gpu::SharedMemoryLimits(), gpu_memory_buffer_manager_,
      image_factory_, nullptr /* gpu_channel_manager_delegate */,
      base::ThreadTaskRunnerHandle::Get());

  if (bind_result_ != gpu::ContextResult::kSuccess)
    return bind_result_;

  cache_controller_ = std::make_unique<viz::ContextCacheController>(
      context_->GetImplementation(), base::ThreadTaskRunnerHandle::Get());

  std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), context_.get());
  context_->GetImplementation()->TraceBeginCHROMIUM(
      "gpu_toplevel", unique_context_name.c_str());

  raster_context_ = std::make_unique<gpu::raster::RasterImplementationGLES>(
      context_->GetImplementation(), context_->GetImplementation(),
      context_->GetImplementation()->capabilities());

  return bind_result_;
}

const gpu::Capabilities& InProcessContextProvider::ContextCapabilities() const {
  CheckValidThreadOrLockAcquired();
  return context_->GetImplementation()->capabilities();
}

const gpu::GpuFeatureInfo& InProcessContextProvider::GetGpuFeatureInfo() const {
  CheckValidThreadOrLockAcquired();
  return context_->GetGpuFeatureInfo();
}

gpu::gles2::GLES2Interface* InProcessContextProvider::ContextGL() {
  CheckValidThreadOrLockAcquired();

  return context_->GetImplementation();
}

gpu::raster::RasterInterface* InProcessContextProvider::RasterInterface() {
  CheckValidThreadOrLockAcquired();

  return raster_context_.get();
}

gpu::ContextSupport* InProcessContextProvider::ContextSupport() {
  CheckValidThreadOrLockAcquired();

  return context_->GetImplementation();
}

class GrContext* InProcessContextProvider::GrContext() {
  CheckValidThreadOrLockAcquired();

  if (gr_context_)
    return gr_context_->get();

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  skia_bindings::GrContextForGLES2Interface::DefaultCacheLimitsForTests(
      &max_resource_cache_bytes, &max_glyph_cache_texture_bytes);
  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(
      ContextGL(), ContextCapabilities(), max_resource_cache_bytes,
      max_glyph_cache_texture_bytes));
  cache_controller_->SetGrContext(gr_context_->get());

  return gr_context_->get();
}

viz::ContextCacheController* InProcessContextProvider::CacheController() {
  CheckValidThreadOrLockAcquired();
  return cache_controller_.get();
}

void InProcessContextProvider::InvalidateGrContext(uint32_t state) {
  CheckValidThreadOrLockAcquired();

  if (gr_context_)
    gr_context_->ResetContext(state);
}

base::Lock* InProcessContextProvider::GetLock() {
  if (!support_locking_)
    return nullptr;
  return &context_lock_;
}

void InProcessContextProvider::AddObserver(viz::ContextLostObserver* obs) {
  // Pixel tests do not test lost context.
}

void InProcessContextProvider::RemoveObserver(viz::ContextLostObserver* obs) {
  // Pixel tests do not test lost context.
}

uint32_t InProcessContextProvider::GetCopyTextureInternalFormat() {
  if (attribs_.alpha_size > 0)
    return GL_RGBA;
  DCHECK_NE(attribs_.red_size, 0);
  DCHECK_NE(attribs_.green_size, 0);
  DCHECK_NE(attribs_.blue_size, 0);
  return GL_RGB;
}

}  // namespace ui
