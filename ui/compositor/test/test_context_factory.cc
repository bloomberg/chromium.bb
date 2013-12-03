// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_context_factory.h"

#include "cc/output/output_surface.h"
#include "cc/test/test_context_provider.h"
#include "ui/compositor/reflector.h"

namespace ui {

TestContextFactory::TestContextFactory() {}

TestContextFactory::~TestContextFactory() {}

scoped_ptr<cc::OutputSurface> TestContextFactory::CreateOutputSurface(
    Compositor* compositor, bool software_fallback) {
  DCHECK(!software_fallback);
  return make_scoped_ptr(
      new cc::OutputSurface(cc::TestContextProvider::Create()));
}

scoped_refptr<Reflector> TestContextFactory::CreateReflector(
    Compositor* mirrored_compositor,
    Layer* mirroring_layer) {
  return new Reflector();
}

void TestContextFactory::RemoveReflector(scoped_refptr<Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
TestContextFactory::OffscreenCompositorContextProvider() {
  if (!offscreen_compositor_contexts_.get() ||
      offscreen_compositor_contexts_->DestroyedOnMainThread())
    offscreen_compositor_contexts_ = cc::TestContextProvider::Create();
  return offscreen_compositor_contexts_;
}

scoped_refptr<cc::ContextProvider>
TestContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->DestroyedOnMainThread())
    return shared_main_thread_contexts_;

  if (ui::Compositor::WasInitializedWithThread()) {
    shared_main_thread_contexts_ = cc::TestContextProvider::Create();
  } else {
    shared_main_thread_contexts_ =
        static_cast<cc::TestContextProvider*>(
            OffscreenCompositorContextProvider().get());
  }
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void TestContextFactory::RemoveCompositor(Compositor* compositor) {
}

bool TestContextFactory::DoesCreateTestContexts() { return true; }

}  // namespace ui
