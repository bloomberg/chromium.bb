/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef PerformanceNavigation_h
#define PerformanceNavigation_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScriptState;
class ScriptValue;

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
class CORE_EXPORT PerformanceNavigation final
    : public GarbageCollected<PerformanceNavigation>,
      public ScriptWrappable,
      public DOMWindowClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceNavigation);

 public:
  static PerformanceNavigation* Create(LocalFrame* frame) {
    return new PerformanceNavigation(frame);
  }

  enum PerformanceNavigationType {
    kTypeNavigate,
    kTypeReload,
    kTypeBackForward,
    kTypeReserved = 255
  };

  unsigned short type() const;
  unsigned short redirectCount() const;

  ScriptValue toJSONForBinding(ScriptState*) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit PerformanceNavigation(LocalFrame*);
};

}  // namespace blink

#endif  // PerformanceNavigation_h
