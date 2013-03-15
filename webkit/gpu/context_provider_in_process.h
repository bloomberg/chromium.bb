// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
#define WEBKIT_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/context_provider.h"
#include "webkit/gpu/webkit_gpu_export.h"

namespace webkit {
namespace gpu {
class GrContextForWebGraphicsContext3D;

class WEBKIT_GPU_EXPORT ContextProviderInProcess
    : NON_EXPORTED_BASE(public cc::ContextProvider) {
 public:
  enum InProcessType {
    IN_PROCESS,
    IN_PROCESS_COMMAND_BUFFER,
  };
  static scoped_refptr<ContextProviderInProcess> Create(InProcessType type) {
    scoped_refptr<ContextProviderInProcess> provider =
        new ContextProviderInProcess(type);
    if (!provider->InitializeOnMainThread())
      return NULL;
    return provider;
  }

  virtual bool BindToCurrentThread() OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;
  virtual bool DestroyedOnMainThread() OVERRIDE;

 protected:
  explicit ContextProviderInProcess(InProcessType type);
  virtual ~ContextProviderInProcess();

  bool InitializeOnMainThread();

  scoped_ptr<WebKit::WebGraphicsContext3D> CreateOffscreenContext3d();

  void OnLostContextInternal();
  virtual void OnLostContext() {}
  virtual void OnMemoryAllocationChanged(bool nonzero_allocation);

 private:
  InProcessType type_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<webkit::gpu::GrContextForWebGraphicsContext3D> gr_context_;

  base::Lock destroyed_lock_;
  bool destroyed_;

  class LostContextCallbackProxy;
  scoped_ptr<LostContextCallbackProxy> lost_context_callback_proxy_;

  class MemoryAllocationCallbackProxy;
  scoped_ptr<MemoryAllocationCallbackProxy> memory_allocation_callback_proxy_;
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
