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

#include "modules/indexeddb/IDBAny.h"

#include "core/dom/DOMStringList.h"
#include "modules/indexeddb/IDBCursorWithValue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

IDBAny* IDBAny::CreateUndefined() {
  return new IDBAny(kUndefinedType);
}

IDBAny* IDBAny::CreateNull() {
  return new IDBAny(kNullType);
}

IDBAny::IDBAny(Type type) : type_(type) {
  DCHECK(type == kUndefinedType || type == kNullType);
}

IDBAny::~IDBAny() {}

void IDBAny::ContextWillBeDestroyed() {
  if (idb_cursor_)
    idb_cursor_->ContextWillBeDestroyed();
}

DOMStringList* IDBAny::DomStringList() const {
  DCHECK_EQ(type_, kDOMStringListType);
  return dom_string_list_.Get();
}

IDBCursor* IDBAny::IdbCursor() const {
  DCHECK_EQ(type_, kIDBCursorType);
  SECURITY_DCHECK(idb_cursor_->IsKeyCursor());
  return idb_cursor_.Get();
}

IDBCursorWithValue* IDBAny::IdbCursorWithValue() const {
  DCHECK_EQ(type_, kIDBCursorWithValueType);
  SECURITY_DCHECK(idb_cursor_->IsCursorWithValue());
  return ToIDBCursorWithValue(idb_cursor_.Get());
}

IDBDatabase* IDBAny::IdbDatabase() const {
  DCHECK_EQ(type_, kIDBDatabaseType);
  return idb_database_.Get();
}

IDBIndex* IDBAny::IdbIndex() const {
  DCHECK_EQ(type_, kIDBIndexType);
  return idb_index_.Get();
}

IDBObjectStore* IDBAny::IdbObjectStore() const {
  DCHECK_EQ(type_, kIDBObjectStoreType);
  return idb_object_store_.Get();
}

const IDBKey* IDBAny::Key() const {
  // If type is IDBValueType then instead use value()->primaryKey().
  DCHECK_EQ(type_, kKeyType);
  return idb_key_.Get();
}

IDBValue* IDBAny::Value() const {
  DCHECK_EQ(type_, kIDBValueType);
  return idb_value_.Get();
}

const Vector<RefPtr<IDBValue>>* IDBAny::Values() const {
  DCHECK_EQ(type_, kIDBValueArrayType);
  return &idb_values_;
}

int64_t IDBAny::Integer() const {
  DCHECK_EQ(type_, kIntegerType);
  return integer_;
}

IDBAny::IDBAny(DOMStringList* value)
    : type_(kDOMStringListType), dom_string_list_(value) {}

IDBAny::IDBAny(IDBCursor* value)
    : type_(value->IsCursorWithValue() ? kIDBCursorWithValueType
                                       : kIDBCursorType),
      idb_cursor_(value) {}

IDBAny::IDBAny(IDBDatabase* value)
    : type_(kIDBDatabaseType), idb_database_(value) {}

IDBAny::IDBAny(IDBIndex* value) : type_(kIDBIndexType), idb_index_(value) {}

IDBAny::IDBAny(IDBObjectStore* value)
    : type_(kIDBObjectStoreType), idb_object_store_(value) {}

IDBAny::IDBAny(const Vector<RefPtr<IDBValue>>& values)
    : type_(kIDBValueArrayType), idb_values_(values) {}

IDBAny::IDBAny(RefPtr<IDBValue> value)
    : type_(kIDBValueType), idb_value_(std::move(value)) {}

IDBAny::IDBAny(IDBKey* key) : type_(kKeyType), idb_key_(key) {}

IDBAny::IDBAny(int64_t value) : type_(kIntegerType), integer_(value) {}

DEFINE_TRACE(IDBAny) {
  visitor->Trace(dom_string_list_);
  visitor->Trace(idb_cursor_);
  visitor->Trace(idb_database_);
  visitor->Trace(idb_index_);
  visitor->Trace(idb_object_store_);
  visitor->Trace(idb_key_);
}

}  // namespace blink
