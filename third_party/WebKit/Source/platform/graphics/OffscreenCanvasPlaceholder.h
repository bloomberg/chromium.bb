// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasPlaceholder_h
#define OffscreenCanvasPlaceholder_h

#include "platform/PlatformExport.h"
#include "wtf/RefPtr.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace blink {

class OffscreenCanvasFrameDispatcher;
class StaticBitmapImage;
class WebTaskRunner;

class PLATFORM_EXPORT OffscreenCanvasPlaceholder {
 public:
  ~OffscreenCanvasPlaceholder();

  virtual void setPlaceholderFrame(RefPtr<StaticBitmapImage>,
                                   WeakPtr<OffscreenCanvasFrameDispatcher>,
                                   RefPtr<WebTaskRunner>,
                                   unsigned resourceId);
  void releasePlaceholderFrame();

  static OffscreenCanvasPlaceholder* getPlaceholderById(unsigned placeholderId);

  void registerPlaceholder(unsigned placeholderId);
  void unregisterPlaceholder();
  const RefPtr<StaticBitmapImage>& placeholderFrame() const {
    return m_placeholderFrame;
  }

 private:
  bool isPlaceholderRegistered() const {
    return m_placeholderId != kNoPlaceholderId;
  }

  RefPtr<StaticBitmapImage> m_placeholderFrame;
  WeakPtr<OffscreenCanvasFrameDispatcher> m_frameDispatcher;
  RefPtr<WebTaskRunner> m_frameDispatcherTaskRunner;
  unsigned m_placeholderFrameResourceId = 0;

  enum {
    kNoPlaceholderId = -1,
  };
  int m_placeholderId = kNoPlaceholderId;
};

}  // blink

#endif
