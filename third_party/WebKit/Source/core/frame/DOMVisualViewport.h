/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMVisualViewport_h
#define DOMVisualViewport_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class LocalDOMWindow;
class ExecutionContext;

class CORE_EXPORT DOMVisualViewport final : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMVisualViewport* Create(LocalDOMWindow* window) {
    return new DOMVisualViewport(window);
  }

  ~DOMVisualViewport() override;

  virtual void Trace(blink::Visitor*);

  // EventTarget overrides:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  float offsetLeft() const;
  float offsetTop() const;
  float pageLeft() const;
  float pageTop() const;
  double width() const;
  double height() const;
  double scale() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(resize);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(scroll);

 private:
  explicit DOMVisualViewport(LocalDOMWindow*);

  Member<LocalDOMWindow> window_;
};

}  // namespace blink

#endif  // DOMVisualViewport_h
