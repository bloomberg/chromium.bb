// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EmbeddedContentView_h
#define EmbeddedContentView_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CullRect;
class GraphicsContext;
class IntRect;
class LayoutEmbeddedContent;

// EmbeddedContentView is a pure virtual class which is implemented by
// LocalFrameView, RemoteFrameView, and PluginView.
class CORE_EXPORT EmbeddedContentView : public GarbageCollectedMixin {
 public:
  virtual ~EmbeddedContentView() {}

  virtual bool IsLocalFrameView() const { return false; }
  virtual bool IsPluginView() const { return false; }

  virtual LayoutEmbeddedContent* OwnerLayoutObject() const = 0;
  virtual void AttachToLayout() = 0;
  virtual void DetachFromLayout() = 0;
  virtual bool IsAttached() const = 0;
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
#endif  // EmbeddedContentView_h
