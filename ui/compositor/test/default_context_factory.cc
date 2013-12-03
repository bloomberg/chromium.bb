// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/default_context_factory.h"

#include "cc/output/output_surface.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace ui {

DefaultContextFactory::DefaultContextFactory() {
}

DefaultContextFactory::~DefaultContextFactory() {
}

bool DefaultContextFactory::Initialize() {
  if (!gfx::GLSurface::InitializeOneOff() ||
      gfx::GetGLImplementation() == gfx::kGLImplementationNone) {
    LOG(ERROR) << "Could not load the GL bindings";
    return false;
  }
  return true;
}

scoped_ptr<cc::OutputSurface> DefaultContextFactory::CreateOutputSurface(
    Compositor* compositor, bool software_fallback) {
  DCHECK(!software_fallback);
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.shareResources = true;

  using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d(
      WebGraphicsContext3DInProcessCommandBufferImpl::CreateViewContext(
          attrs, compositor->widget()));
  CHECK(context3d);

  using webkit::gpu::ContextProviderInProcess;
  scoped_refptr<ContextProviderInProcess> context_provider =
      ContextProviderInProcess::Create(context3d.Pass(),
                                       "UICompositor");

  return make_scoped_ptr(new cc::OutputSurface(context_provider));
}

scoped_refptr<Reflector> DefaultContextFactory::CreateReflector(
    Compositor* mirroed_compositor,
    Layer* mirroring_layer) {
  return NULL;
}

void DefaultContextFactory::RemoveReflector(
    scoped_refptr<Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
DefaultContextFactory::OffscreenCompositorContextProvider() {
  if (!offscreen_compositor_contexts_.get() ||
      !offscreen_compositor_contexts_->DestroyedOnMainThread()) {
    offscreen_compositor_contexts_ =
        webkit::gpu::ContextProviderInProcess::CreateOffscreen();
  }
  return offscreen_compositor_contexts_;
}

scoped_refptr<cc::ContextProvider>
DefaultContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->DestroyedOnMainThread())
    return shared_main_thread_contexts_;

  if (ui::Compositor::WasInitializedWithThread()) {
    shared_main_thread_contexts_ =
        webkit::gpu::ContextProviderInProcess::CreateOffscreen();
  } else {
    shared_main_thread_contexts_ =
        static_cast<webkit::gpu::ContextProviderInProcess*>(
            OffscreenCompositorContextProvider().get());
  }
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void DefaultContextFactory::RemoveCompositor(Compositor* compositor) {
}

bool DefaultContextFactory::DoesCreateTestContexts() { return false; }

}  // namespace ui
