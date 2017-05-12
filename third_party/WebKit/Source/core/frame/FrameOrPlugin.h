// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameOrPlugin_h
#define FrameOrPlugin_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CullRect;
class FrameView;
class GraphicsContext;
class IntRect;

// FrameOrPlugin is a pure virtual class which is implemented by FrameView,
// RemoteFrameView, and PluginView.
class CORE_EXPORT FrameOrPlugin : public GarbageCollectedMixin {
 public:
  virtual ~FrameOrPlugin() {}

  virtual bool IsFrameView() const { return false; }
  virtual bool IsPluginView() const { return false; }

  virtual void SetParent(FrameView*) = 0;
  virtual FrameView* Parent() const = 0;
  virtual void SetParentVisible(bool) = 0;
  virtual void SetFrameRect(const IntRect&) = 0;
  virtual void FrameRectsChanged() = 0;
  virtual const IntRect& FrameRect() const = 0;
  virtual void Paint(GraphicsContext&, const CullRect&) const = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Dispose() = 0;
};

}  // namespace blink
#endif  // FrameOrPlugin_h
