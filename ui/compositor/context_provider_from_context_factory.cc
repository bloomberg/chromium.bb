// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "context_provider_from_context_factory.h"

#include "base/logging.h"

namespace ui {

// static
scoped_refptr<ContextProviderFromContextFactory>
ContextProviderFromContextFactory::CreateForOffscreen(ContextFactory* factory) {
  scoped_refptr<ContextProviderFromContextFactory> provider =
      new ContextProviderFromContextFactory(factory);
  if (!provider->InitializeOnMainThread())
    return NULL;
  return provider;
}

ContextProviderFromContextFactory::ContextProviderFromContextFactory(
    ContextFactory* factory)
    : factory_(factory),
      destroyed_(false) {
}

ContextProviderFromContextFactory::~ContextProviderFromContextFactory() {
}

bool ContextProviderFromContextFactory::BindToCurrentThread() {
  DCHECK(context3d_);
  return context3d_->makeContextCurrent();
}

WebKit::WebGraphicsContext3D* ContextProviderFromContextFactory::Context3d() {
  DCHECK(context3d_);
  return context3d_.get();
}

class GrContext* ContextProviderFromContextFactory::GrContext() {
  DCHECK(context3d_);

  if (!gr_context_) {
    gr_context_.reset(
        new webkit::gpu::GrContextForWebGraphicsContext3D(context3d_.get()));
  }
  return gr_context_->get();
}

void ContextProviderFromContextFactory::VerifyContexts() {
  DCHECK(context3d_);

  if (context3d_->isContextLost()) {
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
  }
}

bool ContextProviderFromContextFactory::DestroyedOnMainThread() {
  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void ContextProviderFromContextFactory::SetLostContextCallback(
    const LostContextCallback& cb) {
  NOTIMPLEMENTED();
}

bool ContextProviderFromContextFactory::InitializeOnMainThread() {
  if (context3d_)
    return true;
  context3d_ = factory_->CreateOffscreenContext();
  return !!context3d_;
}

}  // namespace ui
