// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/PlatformExport.h"
#include "platform/wtf/ThreadSpecific.h"

#include <memory>

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}
class GrContext;

namespace blink {

class WaitableEvent;
class WebGraphicsContext3DProvider;

// SharedGpuContext provides access to a thread-specific GPU context
// that is shared by many callsites throughout the thread.
// When on the main thread, provides access to the same context as
// Platform::createSharedOffscreenGraphicsContext3DProvider
class PLATFORM_EXPORT SharedGpuContext {
 public:
  // The contextId is incremented each time a new underlying context
  // is created. For example, when the context is lost, then restored.
  // User code can rely on this Id to determine whether long-lived
  // gpu resources are still alive in the current context.
  static unsigned ContextId();
  static gpu::gles2::GLES2Interface*
  Gl();                    // May re-create context if context was lost
  static GrContext* Gr();  // May re-create context if context was lost
  static bool IsValid();   // May re-create context if context was lost
  static bool IsValidWithoutRestoring();
  typedef std::function<std::unique_ptr<WebGraphicsContext3DProvider>()>
      ContextProviderFactory;
  static void SetContextProviderFactoryForTesting(ContextProviderFactory);

  enum {
    kNoSharedContext = 0,
  };

 private:
  static SharedGpuContext* GetInstanceForCurrentThread();

  SharedGpuContext();
  void CreateContextProviderOnMainThread(WaitableEvent*);
  void CreateContextProviderIfNeeded();

  ContextProviderFactory context_provider_factory_ = nullptr;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider_;
  unsigned context_id_;
  friend class WTF::ThreadSpecific<SharedGpuContext>;
};

}  // blink
