// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_factory.h"

#include "base/command_line.h"
#include "base/threading/thread.h"
#include "cc/output/output_surface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace ui {

InProcessContextFactory::InProcessContextFactory()
    : shared_bitmap_manager_(new cc::TestSharedBitmapManager()) {
  DCHECK_NE(gfx::GetGLImplementation(), gfx::kGLImplementationNone);
#if defined(OS_CHROMEOS)
  bool use_thread = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUIDisableThreadedCompositing);
#else
  bool use_thread = false;
#endif
  if (use_thread) {
    compositor_thread_.reset(new base::Thread("Browser Compositor"));
    compositor_thread_->Start();
  }
}

InProcessContextFactory::~InProcessContextFactory() {}

scoped_ptr<cc::OutputSurface> InProcessContextFactory::CreateOutputSurface(
    Compositor* compositor,
    bool software_fallback) {
  DCHECK(!software_fallback);
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.shareResources = true;
  bool lose_context_when_out_of_memory = true;

  using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d(
      WebGraphicsContext3DInProcessCommandBufferImpl::CreateViewContext(
          attrs, lose_context_when_out_of_memory, compositor->widget()));
  CHECK(context3d);

  using webkit::gpu::ContextProviderInProcess;
  scoped_refptr<ContextProviderInProcess> context_provider =
      ContextProviderInProcess::Create(context3d.Pass(), "UICompositor");

  return make_scoped_ptr(new cc::OutputSurface(context_provider));
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
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->DestroyedOnMainThread())
    return shared_main_thread_contexts_;

  bool lose_context_when_out_of_memory = false;
  shared_main_thread_contexts_ =
      webkit::gpu::ContextProviderInProcess::CreateOffscreen(
          lose_context_when_out_of_memory);
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void InProcessContextFactory::RemoveCompositor(Compositor* compositor) {}

bool InProcessContextFactory::DoesCreateTestContexts() { return false; }

cc::SharedBitmapManager* InProcessContextFactory::GetSharedBitmapManager() {
  return shared_bitmap_manager_.get();
}

base::MessageLoopProxy* InProcessContextFactory::GetCompositorMessageLoop() {
  if (!compositor_thread_)
    return NULL;
  return compositor_thread_->message_loop_proxy();
}

}  // namespace ui
