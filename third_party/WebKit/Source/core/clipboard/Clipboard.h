// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Clipboard_h
#define Clipboard_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class DataTransfer;
class ScriptState;

class Clipboard : public EventTargetWithInlineData,
                  public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Clipboard);
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(Clipboard);

 public:
  explicit Clipboard(ExecutionContext*);

  ScriptPromise read(ScriptState*);
  ScriptPromise readText(ScriptState*);

  ScriptPromise write(ScriptState*, DataTransfer*);
  ScriptPromise writeText(ScriptState*, const String&);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  virtual void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // Clipboard_h
