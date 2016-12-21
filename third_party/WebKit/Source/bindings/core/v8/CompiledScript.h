// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompiledScript_h
#define CompiledScript_h

#include "bindings/core/v8/ScriptSourceCode.h"
#include "platform/heap/GarbageCollected.h"
#include "wtf/Assertions.h"
#include "wtf/text/TextPosition.h"

#include <v8.h>

namespace blink {

// This wraps a global handle on a V8 object (the compiled script).
// Because that script may contain further references to other objects, do not
// keep long-lasting references to this object, as they may induce cycles
// between V8 and Oilpan.
class CompiledScript : public GarbageCollectedFinalized<CompiledScript> {
 public:
  CompiledScript(v8::Isolate* isolate,
                 v8::Local<v8::Script> script,
                 const ScriptSourceCode& sourceCode)
      : m_sourceCode(sourceCode), m_script(isolate, script) {
    DCHECK(!script.IsEmpty());
  }

  v8::Local<v8::Script> script(v8::Isolate* isolate) const {
    return m_script.Get(isolate);
  }

  const ScriptSourceCode& sourceCode() const { return m_sourceCode; }
  const KURL& url() const { return m_sourceCode.url(); }
  const TextPosition& startPosition() const {
    return m_sourceCode.startPosition();
  }

  DEFINE_INLINE_TRACE() { visitor->trace(m_sourceCode); }

 private:
  ScriptSourceCode m_sourceCode;
  v8::Global<v8::Script> m_script;
};

}  // namespace blink

#endif  // CompiledScript_h
