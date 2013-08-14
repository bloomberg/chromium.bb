// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
#define WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "webkit/common/gpu/webkit_gpu_export.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace webkit {
namespace gpu {
class GrContextForWebGraphicsContext3D;
class WebGraphicsContext3DInProcessCommandBufferImpl;

class WEBKIT_GPU_EXPORT ContextProviderInProcess
    : NON_EXPORTED_BASE(public cc::ContextProvider) {
 public:
  typedef base::Callback<
      scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(void)>
      CreateCallback;

  static scoped_refptr<ContextProviderInProcess> Create(
      const CreateCallback& create_callback);

  // Calls Create() with a default factory method for creating an offscreen
  // context.
  static scoped_refptr<ContextProviderInProcess> CreateOffscreen();

  virtual bool BindToCurrentThread() OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;
  virtual bool DestroyedOnMainThread() OVERRIDE;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) OVERRIDE;

 protected:
  ContextProviderInProcess();
  virtual ~ContextProviderInProcess();

  bool InitializeOnMainThread(
      const CreateCallback& create_callback);

  void OnLostContext();
  void OnMemoryAllocationChanged(bool nonzero_allocation);

 private:
  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<webkit::gpu::GrContextForWebGraphicsContext3D> gr_context_;

  LostContextCallback lost_context_callback_;

  base::Lock destroyed_lock_;
  bool destroyed_;

  class LostContextCallbackProxy;
  scoped_ptr<LostContextCallbackProxy> lost_context_callback_proxy_;

  class MemoryAllocationCallbackProxy;
  scoped_ptr<MemoryAllocationCallbackProxy> memory_allocation_callback_proxy_;
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
