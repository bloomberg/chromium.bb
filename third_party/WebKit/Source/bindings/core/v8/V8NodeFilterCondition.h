/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#ifndef V8NodeFilterCondition_h
#define V8NodeFilterCondition_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class Node;
class ScriptState;

// V8NodeFilterCondition maintains a Javascript implemented callback for
// filtering Node returned by NodeIterator/TreeWalker.  A V8NodeFilterCondition
// is referenced by a NodeIterator/TreeWalker.
//
// Binding generator should generate this code.  See crbug.com/630986.
class V8NodeFilterCondition final
    : public GarbageCollectedFinalized<V8NodeFilterCondition>,
      public TraceWrapperBase {
 public:
  static V8NodeFilterCondition* CreateOrNull(v8::Local<v8::Value> filter,
                                             ScriptState* script_state) {
    return filter->IsNull() ? nullptr
                            : new V8NodeFilterCondition(filter, script_state);
  }

  ~V8NodeFilterCondition();
  virtual void Trace(blink::Visitor* visitor) {}
  DECLARE_TRACE_WRAPPERS();

  unsigned acceptNode(Node*, ExceptionState&) const;
  v8::Local<v8::Value> Callback(v8::Isolate* isolate) const {
    return filter_.NewLocal(isolate);
  }

 private:
  V8NodeFilterCondition(v8::Local<v8::Value> filter, ScriptState*);

  RefPtr<ScriptState> script_state_;
  TraceWrapperV8Reference<v8::Object> filter_;
};

}  // namespace blink

#endif  // V8NodeFilterCondition_h
