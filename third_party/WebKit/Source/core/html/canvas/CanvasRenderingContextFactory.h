// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasRenderingContextFactory_h
#define CanvasRenderingContextFactory_h

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/html/canvas/CanvasContextCreationAttributesCore.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class HTMLCanvasElement;
class OffscreenCanvas;

class CORE_EXPORT CanvasRenderingContextFactory {
  USING_FAST_MALLOC(CanvasRenderingContextFactory);

 public:
  CanvasRenderingContextFactory() = default;
  virtual ~CanvasRenderingContextFactory() = default;

  virtual CanvasRenderingContext* Create(
      CanvasRenderingContextHost*,
      const CanvasContextCreationAttributesCore&) = 0;

  virtual CanvasRenderingContext::ContextType GetContextType() const = 0;
  virtual void OnError(HTMLCanvasElement*, const String& error){};
  virtual void OnError(OffscreenCanvas*, const String& error){};

  DISALLOW_COPY_AND_ASSIGN(CanvasRenderingContextFactory);
};

}  // namespace blink

#endif  // CanvasRenderingContextFactory_h
