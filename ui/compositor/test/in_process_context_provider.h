// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
#define WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GLInProcessContext;
}

namespace ui {

class InProcessContextProvider : public cc::ContextProvider {
 public:
  static scoped_refptr<InProcessContextProvider> Create(
      const gpu::gles2::ContextCreationAttribHelper& attribs,
      bool lose_context_when_out_of_memory,
      gfx::AcceleratedWidget window,
      const std::string& debug_name);

  // Uses default attributes for creating an offscreen context.
  static scoped_refptr<InProcessContextProvider> CreateOffscreen(
      bool lose_context_when_out_of_memory);

 private:
  InProcessContextProvider(
      const gpu::gles2::ContextCreationAttribHelper& attribs,
      bool lose_context_when_out_of_memory,
      gfx::AcceleratedWidget window,
      const std::string& debug_name);
  ~InProcessContextProvider() override;

  // cc::ContextProvider:
  bool BindToCurrentThread() override;
  Capabilities ContextCapabilities() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void SetupLock() override;
  base::Lock* GetLock() override;
  bool IsContextLost() override;
  void VerifyContexts() override;
  void DeleteCachedResources() override;
  bool DestroyedOnMainThread() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
  void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override;

  void OnLostContext();

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  scoped_ptr<gpu::GLInProcessContext> context_;
  skia::RefPtr<class GrContext> gr_context_;

  gpu::gles2::ContextCreationAttribHelper attribs_;
  bool lose_context_when_out_of_memory_;
  gfx::AcceleratedWidget window_;
  std::string debug_name_;
  cc::ContextProvider::Capabilities capabilities_;

  LostContextCallback lost_context_callback_;

  base::Lock destroyed_lock_;
  bool destroyed_;

  base::Lock context_lock_;

  DISALLOW_COPY_AND_ASSIGN(InProcessContextProvider);
};

}  // namespace ui

#endif  // WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_IN_PROCESS_H_
