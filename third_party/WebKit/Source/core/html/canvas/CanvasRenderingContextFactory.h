// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasRenderingContextFactory_h
#define CanvasRenderingContextFactory_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PassRefPtr.h"

namespace blink {

class HTMLCanvasElement;
class OffscreenCanvas;

class CORE_EXPORT CanvasRenderingContextFactory {
  USING_FAST_MALLOC(CanvasRenderingContextFactory);
  WTF_MAKE_NONCOPYABLE(CanvasRenderingContextFactory);

 public:
  CanvasRenderingContextFactory() = default;
  virtual ~CanvasRenderingContextFactory() {}

  virtual CanvasRenderingContext* Create(
      CanvasRenderingContextHost*,
      const CanvasContextCreationAttributes&) = 0;

  virtual CanvasRenderingContext::ContextType GetContextType() const = 0;
  virtual void OnError(HTMLCanvasElement*, const String& error){};
  virtual void OnError(OffscreenCanvas*, const String& error){};
};

}  // namespace blink

#endif  // CanvasRenderingContextFactory_h
