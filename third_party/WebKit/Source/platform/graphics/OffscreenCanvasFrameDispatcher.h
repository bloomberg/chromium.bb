// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcher_h
#define OffscreenCanvasFrameDispatcher_h

#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "wtf/RefPtr.h"
#include "wtf/WeakPtr.h"

namespace blink {

class StaticBitmapImage;

class OffscreenCanvasFrameDispatcherClient {
 public:
  virtual void beginFrame() = 0;
};

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcher {
 public:
  virtual ~OffscreenCanvasFrameDispatcher() {}
  virtual void dispatchFrame(RefPtr<StaticBitmapImage>,
                             double commitStartTime,
                             bool isWebGLSoftwareRendering) = 0;
  virtual void setNeedsBeginFrame(bool) = 0;
  virtual void reclaimResource(unsigned resourceId) = 0;

  virtual void reshape(int width, int height) = 0;

  WeakPtr<OffscreenCanvasFrameDispatcher> createWeakPtr() {
    return m_weakPtrFactory.createWeakPtr();
  }

  OffscreenCanvasFrameDispatcherClient* client() { return m_client; }

 protected:
  OffscreenCanvasFrameDispatcher(OffscreenCanvasFrameDispatcherClient* client)
      : m_weakPtrFactory(this), m_client(client) {}

 private:
  WeakPtrFactory<OffscreenCanvasFrameDispatcher> m_weakPtrFactory;
  OffscreenCanvasFrameDispatcherClient* m_client;
};

}  // namespace blink

#endif  // OffscreenCanvasFrameDispatcher_h
