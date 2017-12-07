// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SharedGpuContext_h
#define SharedGpuContext_h

#include <memory>
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/wtf/ThreadSpecific.h"

namespace blink {

class WebGraphicsContext3DProvider;

// SharedGpuContext provides access to a thread-specific GPU context
// that is shared by many callsites throughout the thread.
// When on the main thread, provides access to the same context as
// Platform::CreateSharedOffscreenGraphicsContext3DProvider, and the
// same query as Platform::IsGPUCompositingEnabled().
class PLATFORM_EXPORT SharedGpuContext {
 public:
  // Thread-safe query if gpu compositing is enabled. This should be done before
  // calling ContextProviderWrapper() if the context will be used to make
  // resources meant for the compositor. When it is false, no context will be
  // needed and software-based resources should be given to the compositor
  // instead.
  static bool IsGpuCompositingEnabled();
  // May re-create context if context was lost
  static base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper();
  static bool AllowSoftwareToAcceleratedCanvasUpgrade();
  static bool IsValidWithoutRestoring();

  using ContextProviderFactory =
      base::RepeatingCallback<std::unique_ptr<WebGraphicsContext3DProvider>(
          bool* is_gpu_compositing_disabled)>;
  static void SetContextProviderFactoryForTesting(ContextProviderFactory);
  // Resets the global instance including the |context_provider_factory_| and
  // dropping the context. Should be called at the end of a test that uses this
  // to not interfere with the next test.
  static void ResetForTesting();

 private:
  friend class WTF::ThreadSpecific<SharedGpuContext>;

  static SharedGpuContext* GetInstanceForCurrentThread();

  SharedGpuContext();
  void CreateContextProviderIfNeeded(bool only_if_gpu_compositing);

  // Can be overridden for tests.
  ContextProviderFactory context_provider_factory_;

  // This is sticky once true, we never need to ask again.
  bool is_gpu_compositing_disabled_ = false;
  std::unique_ptr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
};

}  // blink

#endif  // SharedGpuContext_h
