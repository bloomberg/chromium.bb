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

#ifndef IDBAny_h
#define IDBAny_h

#include "core/dom/DOMStringList.h"
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class IDBCursor;
class IDBCursorWithValue;
class IDBDatabase;
class IDBIndex;
class IDBObjectStore;

// IDBAny is used for:
//  * source of IDBCursor (IDBObjectStore or IDBIndex)
//  * source of IDBRequest (IDBObjectStore, IDBIndex, IDBCursor, or null)
//  * result of IDBRequest (IDBDatabase, IDBCursor, DOMStringList, undefined,
//    integer, key or value)
//
// This allows for lazy conversion to script values (via IDBBindingUtilities),
// and avoids the need for many dedicated union types.

class MODULES_EXPORT IDBAny : public GarbageCollectedFinalized<IDBAny> {
 public:
  static IDBAny* CreateUndefined();
  static IDBAny* CreateNull();
  template <typename T>
  static IDBAny* Create(T* idb_object) {
    return new IDBAny(idb_object);
  }
  static IDBAny* Create(DOMStringList* dom_string_list) {
    return new IDBAny(dom_string_list);
  }
  static IDBAny* Create(int64_t value) { return new IDBAny(value); }
  static IDBAny* Create(RefPtr<IDBValue> value) {
    return new IDBAny(std::move(value));
  }
  static IDBAny* Create(const Vector<RefPtr<IDBValue>>& values) {
    return new IDBAny(values);
  }
  ~IDBAny();
  void Trace(blink::Visitor*);
  void ContextWillBeDestroyed();

  enum Type {
    kUndefinedType = 0,
    kNullType,
    kDOMStringListType,
    kIDBCursorType,
    kIDBCursorWithValueType,
    kIDBDatabaseType,
    kIDBIndexType,
    kIDBObjectStoreType,
    kIntegerType,
    kKeyType,
    kIDBValueType,
    kIDBValueArrayType
  };

  Type GetType() const { return type_; }
  // Use type() to figure out which one of these you're allowed to call.
  DOMStringList* DomStringList() const;
  IDBCursor* IdbCursor() const;
  IDBCursorWithValue* IdbCursorWithValue() const;
  IDBDatabase* IdbDatabase() const;
  IDBIndex* IdbIndex() const;
  IDBObjectStore* IdbObjectStore() const;
  IDBValue* Value() const;
  const Vector<RefPtr<IDBValue>>* Values() const;
  int64_t Integer() const;
  const IDBKey* Key() const;

 private:
  explicit IDBAny(Type);
  explicit IDBAny(DOMStringList*);
  explicit IDBAny(IDBCursor*);
  explicit IDBAny(IDBDatabase*);
  explicit IDBAny(IDBIndex*);
  explicit IDBAny(IDBObjectStore*);
  explicit IDBAny(IDBKey*);
  explicit IDBAny(const Vector<RefPtr<IDBValue>>&);
  explicit IDBAny(RefPtr<IDBValue>);
  explicit IDBAny(int64_t);

  const Type type_;

  // Only one of the following should ever be in use at any given time.
  const Member<DOMStringList> dom_string_list_;
  const Member<IDBCursor> idb_cursor_;
  const Member<IDBDatabase> idb_database_;
  const Member<IDBIndex> idb_index_;
  const Member<IDBObjectStore> idb_object_store_;
  const Member<IDBKey> idb_key_;
  const RefPtr<IDBValue> idb_value_;
  const Vector<RefPtr<IDBValue>> idb_values_;
  const int64_t integer_ = 0;
};

}  // namespace blink

#endif  // IDBAny_h
