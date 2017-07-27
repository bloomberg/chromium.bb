// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SharedGpuContext_h
#define SharedGpuContext_h

#include "platform/PlatformExport.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/wtf/ThreadSpecific.h"

#include <memory>

namespace blink {

class WaitableEvent;
class WebGraphicsContext3DProvider;

// SharedGpuContext provides access to a thread-specific GPU context
// that is shared by many callsites throughout the thread.
// When on the main thread, provides access to the same context as
// Platform::createSharedOffscreenGraphicsContext3DProvider
class PLATFORM_EXPORT SharedGpuContext {
 public:
  // May re-create context if context was lost
  static WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper();
  static bool AllowSoftwareToAcceleratedCanvasUpgrade();

  static bool IsValidWithoutRestoring();
  typedef std::function<std::unique_ptr<WebGraphicsContext3DProvider>()>
      ContextProviderFactory;
  static void SetContextProviderFactoryForTesting(ContextProviderFactory);

 private:
  static SharedGpuContext* GetInstanceForCurrentThread();

  SharedGpuContext();
  void CreateContextProviderOnMainThread(WaitableEvent*);
  void CreateContextProviderIfNeeded();

  ContextProviderFactory context_provider_factory_ = nullptr;
  std::unique_ptr<WebGraphicsContext3DProviderWrapper> context_provider_;
  friend class WTF::ThreadSpecific<SharedGpuContext>;
};

}  // blink

#endif  // SharedGpuContext_h
