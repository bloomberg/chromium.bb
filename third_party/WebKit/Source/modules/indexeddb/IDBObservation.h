// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.+

#ifndef IDBObservation_h
#define IDBObservation_h

#include "bindings/core/v8/ScriptValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class IDBKeyRange;
class IDBValue;
class ScriptState;
struct WebIDBObservation;

class IDBObservation final : public GarbageCollectedFinalized<IDBObservation>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebIDBOperationType StringToOperationType(const String&);
  static IDBObservation* Create(const WebIDBObservation&, v8::Isolate*);
  ~IDBObservation();

  void Trace(blink::Visitor*);

  // Implement the IDL
  ScriptValue key(ScriptState*);
  ScriptValue value(ScriptState*);
  const String& type() const;

 private:
  IDBObservation(const WebIDBObservation&, v8::Isolate*);
  Member<IDBKeyRange> key_range_;
  RefPtr<IDBValue> value_;
  const WebIDBOperationType operation_type_;
};

}  // namespace blink

#endif  // IDBObservation_h
