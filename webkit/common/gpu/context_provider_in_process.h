// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
#define WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "webkit/common/gpu/context_provider_web_context.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "webkit/common/gpu/webkit_gpu_export.h"

namespace blink { class WebGraphicsContext3D; }

namespace webkit {
namespace gpu {
class GrContextForWebGraphicsContext3D;

class WEBKIT_GPU_EXPORT ContextProviderInProcess
    : NON_EXPORTED_BASE(public ContextProviderWebContext) {
 public:
  static scoped_refptr<ContextProviderInProcess> Create(
      scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
      const std::string& debug_name);

  // Uses default attributes for creating an offscreen context.
  static scoped_refptr<ContextProviderInProcess> CreateOffscreen(
      bool lose_context_when_out_of_memory);

  virtual blink::WebGraphicsContext3D* WebContext3D() override;

  virtual bool BindToCurrentThread() override;
  virtual Capabilities ContextCapabilities() override;
  virtual ::gpu::gles2::GLES2Interface* ContextGL() override;
  virtual ::gpu::ContextSupport* ContextSupport() override;
  virtual class GrContext* GrContext() override;
  virtual bool IsContextLost() override;
  virtual void VerifyContexts() override;
  virtual void DeleteCachedResources() override;
  virtual bool DestroyedOnMainThread() override;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override;

 protected:
  ContextProviderInProcess(
      scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
      const std::string& debug_name);
  virtual ~ContextProviderInProcess();

  void OnLostContext();

 private:
  void InitializeCapabilities();

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl>
      context3d_;
  scoped_ptr<webkit::gpu::GrContextForWebGraphicsContext3D> gr_context_;

  LostContextCallback lost_context_callback_;

  base::Lock destroyed_lock_;
  bool destroyed_;

  std::string debug_name_;
  class LostContextCallbackProxy;
  scoped_ptr<LostContextCallbackProxy> lost_context_callback_proxy_;

  cc::ContextProvider::Capabilities capabilities_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderInProcess);
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
