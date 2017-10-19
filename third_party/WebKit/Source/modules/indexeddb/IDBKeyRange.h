/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBKeyRange_h
#define IDBKeyRange_h

#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBKey.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptState;
class ScriptValue;

class MODULES_EXPORT IDBKeyRange final : public GarbageCollected<IDBKeyRange>,
                                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum LowerBoundType { kLowerBoundOpen, kLowerBoundClosed };
  enum UpperBoundType { kUpperBoundOpen, kUpperBoundClosed };

  static IDBKeyRange* Create(IDBKey* lower,
                             IDBKey* upper,
                             LowerBoundType lower_type,
                             UpperBoundType upper_type) {
    return new IDBKeyRange(lower, upper, lower_type, upper_type);
  }
  // Null if the script value is null or undefined, the range if it is one,
  // otherwise tries to convert to a key and throws if it fails.
  static IDBKeyRange* FromScriptValue(ExecutionContext*,
                                      const ScriptValue&,
                                      ExceptionState&);

  void Trace(blink::Visitor*);

  // Implement the IDBKeyRange IDL
  IDBKey* Lower() const { return lower_.Get(); }
  IDBKey* Upper() const { return upper_.Get(); }

  ScriptValue lowerValue(ScriptState*) const;
  ScriptValue upperValue(ScriptState*) const;
  bool lowerOpen() const { return lower_type_ == kLowerBoundOpen; }
  bool upperOpen() const { return upper_type_ == kUpperBoundOpen; }

  static IDBKeyRange* only(ScriptState*,
                           const ScriptValue& key,
                           ExceptionState&);
  static IDBKeyRange* lowerBound(ScriptState*,
                                 const ScriptValue& bound,
                                 bool open,
                                 ExceptionState&);
  static IDBKeyRange* upperBound(ScriptState*,
                                 const ScriptValue& bound,
                                 bool open,
                                 ExceptionState&);
  static IDBKeyRange* bound(ScriptState*,
                            const ScriptValue& lower,
                            const ScriptValue& upper,
                            bool lower_open,
                            bool upper_open,
                            ExceptionState&);

  static IDBKeyRange* only(IDBKey* value, ExceptionState&);

  bool includes(ScriptState*, const ScriptValue& key, ExceptionState&);

 private:
  IDBKeyRange(IDBKey* lower,
              IDBKey* upper,
              LowerBoundType lower_type,
              UpperBoundType upper_type);

  Member<IDBKey> lower_;
  Member<IDBKey> upper_;
  const LowerBoundType lower_type_;
  const UpperBoundType upper_type_;
};

}  // namespace blink

#endif  // IDBKeyRange_h
