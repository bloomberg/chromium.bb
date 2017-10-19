/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef V0CustomElementDefinition_h
#define V0CustomElementDefinition_h

#include "core/html/custom/V0CustomElementDescriptor.h"
#include "core/html/custom/V0CustomElementLifecycleCallbacks.h"

namespace blink {

class V0CustomElementDefinition final
    : public GarbageCollectedFinalized<V0CustomElementDefinition> {
 public:
  static V0CustomElementDefinition* Create(const V0CustomElementDescriptor&,
                                           V0CustomElementLifecycleCallbacks*);

  const V0CustomElementDescriptor& Descriptor() const { return descriptor_; }
  V0CustomElementLifecycleCallbacks* Callbacks() const {
    return callbacks_.Get();
  }

  void Trace(blink::Visitor*);

 private:
  V0CustomElementDefinition(const V0CustomElementDescriptor&,
                            V0CustomElementLifecycleCallbacks*);

  V0CustomElementDescriptor descriptor_;
  Member<V0CustomElementLifecycleCallbacks> callbacks_;
};

}  // namespace blink

#endif  // V0CustomElementDefinition_h
