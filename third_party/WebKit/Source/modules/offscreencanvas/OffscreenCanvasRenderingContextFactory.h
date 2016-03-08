// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasRenderingContextFactory_h
#define OffscreenCanvasRenderingContextFactory_h

#include "wtf/Allocator.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class CanvasContextCreationAttributes;
class OffscreenCanvas;
class OffscreenCanvasRenderingContext;

class OffscreenCanvasRenderingContextFactory {
    USING_FAST_MALLOC(OffscreenCanvasRenderingContextFactory);
    WTF_MAKE_NONCOPYABLE(OffscreenCanvasRenderingContextFactory);
public:
    OffscreenCanvasRenderingContextFactory() = default;
    virtual ~OffscreenCanvasRenderingContextFactory() { }

    virtual OffscreenCanvasRenderingContext* create(OffscreenCanvas*, const CanvasContextCreationAttributes&) = 0;
    virtual OffscreenCanvasRenderingContext::ContextType contextType() const = 0;
    virtual void onError(OffscreenCanvas*, const String& error) = 0;
};

} // namespace blink

#endif // OffscreenCanvasRenderingContextFactory_h
