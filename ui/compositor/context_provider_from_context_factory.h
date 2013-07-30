// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CONTEXT_PROVIDER_FROM_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_CONTEXT_PROVIDER_FROM_CONTEXT_FACTORY_H_

#include "base/synchronization/lock.h"
#include "cc/output/context_provider.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/compositor/compositor.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"

namespace ui {

class ContextProviderFromContextFactory
    : public cc::ContextProvider {
 public:
  static scoped_refptr<ContextProviderFromContextFactory> CreateForOffscreen(
      ContextFactory* factory);

  virtual bool BindToCurrentThread() OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;
  virtual bool DestroyedOnMainThread() OVERRIDE;
  virtual void SetLostContextCallback(const LostContextCallback& cb) OVERRIDE;

 protected:
  ContextProviderFromContextFactory(ContextFactory* factory);
  virtual ~ContextProviderFromContextFactory();

  bool InitializeOnMainThread();

 private:
  ContextFactory* factory_;
  base::Lock destroyed_lock_;
  bool destroyed_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<webkit::gpu::GrContextForWebGraphicsContext3D> gr_context_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CONTEXT_PROVIDER_FROM_CONTEXT_FACTORY_H_
