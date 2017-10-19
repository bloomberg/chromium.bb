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

#ifndef IDBCursor_h
#define IDBCursor_h

#include <memory>
#include "bindings/core/v8/ScriptValue.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class ExceptionState;
class IDBAny;
class IDBTransaction;
class IDBValue;
class ScriptState;

class IDBCursor : public GarbageCollectedFinalized<IDBCursor>,
                  public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebIDBCursorDirection StringToDirection(const String& mode_string);

  static IDBCursor* Create(std::unique_ptr<WebIDBCursor>,
                           WebIDBCursorDirection,
                           IDBRequest*,
                           IDBAny* source,
                           IDBTransaction*);
  virtual ~IDBCursor();
  void Trace(blink::Visitor*);
  void ContextWillBeDestroyed() { backend_.reset(); }

  WARN_UNUSED_RESULT v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

  // Implement the IDL
  const String& direction() const;
  ScriptValue key(ScriptState*);
  ScriptValue primaryKey(ScriptState*);
  ScriptValue value(ScriptState*);
  ScriptValue source(ScriptState*) const;

  IDBRequest* update(ScriptState*, const ScriptValue&, ExceptionState&);
  void advance(unsigned, ExceptionState&);
  void continueFunction(ScriptState*, const ScriptValue& key, ExceptionState&);
  void continuePrimaryKey(ScriptState*,
                          const ScriptValue& key,
                          const ScriptValue& primary_key,
                          ExceptionState&);
  IDBRequest* deleteFunction(ScriptState*, ExceptionState&);

  bool isKeyDirty() const { return key_dirty_; }
  bool isPrimaryKeyDirty() const { return primary_key_dirty_; }
  bool isValueDirty() const { return value_dirty_; }

  void Continue(IDBKey*,
                IDBKey* primary_key,
                IDBRequest::AsyncTraceState,
                ExceptionState&);
  void PostSuccessHandlerCallback();
  bool IsDeleted() const;
  void Close();
  void SetValueReady(IDBKey*, IDBKey* primary_key, RefPtr<IDBValue>);
  IDBKey* IdbPrimaryKey() const { return primary_key_; }
  virtual bool IsKeyCursor() const { return true; }
  virtual bool IsCursorWithValue() const { return false; }

 protected:
  IDBCursor(std::unique_ptr<WebIDBCursor>,
            WebIDBCursorDirection,
            IDBRequest*,
            IDBAny* source,
            IDBTransaction*);

 private:
  IDBObjectStore* EffectiveObjectStore() const;

  std::unique_ptr<WebIDBCursor> backend_;
  Member<IDBRequest> request_;
  const WebIDBCursorDirection direction_;
  Member<IDBAny> source_;
  Member<IDBTransaction> transaction_;
  bool got_value_ = false;
  bool key_dirty_ = true;
  bool primary_key_dirty_ = true;
  bool value_dirty_ = true;
  Member<IDBKey> key_;
  Member<IDBKey> primary_key_;
  RefPtr<IDBValue> value_;
};

}  // namespace blink

#endif  // IDBCursor_h
