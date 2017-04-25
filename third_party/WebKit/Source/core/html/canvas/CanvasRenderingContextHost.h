// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasRenderingContextHost_h
#define CanvasRenderingContextHost_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "core/events/EventDispatcher.h"
#include "core/events/EventTarget.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class StaticBitmapImage;
class KURL;

class CORE_EXPORT CanvasRenderingContextHost : public GarbageCollectedMixin {
 public:
  CanvasRenderingContextHost();

  virtual void DetachContext() = 0;

  virtual void DidDraw(const FloatRect& rect) {}
  virtual void DidDraw() {}

  virtual void FinalizeFrame() = 0;

  virtual bool OriginClean() const = 0;
  virtual void SetOriginTainted() = 0;
  virtual const IntSize& Size() const = 0;

  virtual ExecutionContext* GetTopExecutionContext() const = 0;
  virtual DispatchEventResult HostDispatchEvent(Event*) = 0;
  virtual const KURL& GetExecutionContextUrl() const = 0;

  virtual ScriptPromise Commit(RefPtr<StaticBitmapImage>,
                               bool is_web_gl_software_rendering,
                               ScriptState*,
                               ExceptionState&);

  // TODO(fserb): remove this.
  virtual bool IsOffscreenCanvas() const { return false; }

 protected:
  virtual ~CanvasRenderingContextHost() {}
};

}  // namespace blink

#endif  // CanvasRenderingContextHost_h
