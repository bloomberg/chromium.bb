/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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

#ifndef IDBMetadata_h
#define IDBMetadata_h

#include "modules/indexeddb/IDBKeyPath.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/indexeddb/WebIDBMetadata.h"

namespace blink {

// The lifecycle of the IndexedDB metadata objects defined below is managed by
// reference counting (RefPtr). We don't have to worry about cycles because
// these objects form a tree with the hierarchy shown below.
//     IDBDatabaseMetadata -> IDBObjectStoreMetadata -> IDBIndexMetadata

class IDBIndexMetadata : public RefCounted<IDBIndexMetadata> {
  USING_FAST_MALLOC(IDBIndexMetadata);

 public:
  static constexpr int64_t kInvalidId = -1;

  IDBIndexMetadata();
  IDBIndexMetadata(const String& name,
                   int64_t id,
                   const IDBKeyPath&,
                   bool unique,
                   bool multi_entry);

  String name;
  int64_t id;
  IDBKeyPath key_path;
  bool unique;
  bool multi_entry;
};

class MODULES_EXPORT IDBObjectStoreMetadata
    : public RefCounted<IDBObjectStoreMetadata> {
  USING_FAST_MALLOC(IDBObjectStoreMetadata);

 public:
  static constexpr int64_t kInvalidId = -1;

  IDBObjectStoreMetadata();
  IDBObjectStoreMetadata(const String& name,
                         int64_t id,
                         const IDBKeyPath&,
                         bool auto_increment,
                         int64_t max_index_id);

  // Creates a deep copy of the object metadata, which includes copies of index
  // metadata items.
  RefPtr<IDBObjectStoreMetadata> CreateCopy() const;

  String name;
  int64_t id;
  IDBKeyPath key_path;
  bool auto_increment;
  int64_t max_index_id;
  HashMap<int64_t, RefPtr<IDBIndexMetadata>> indexes;
};

struct MODULES_EXPORT IDBDatabaseMetadata {
  DISALLOW_NEW();

  // FIXME: These can probably be collapsed into 0.
  enum { kNoVersion = -1, kDefaultVersion = 0 };

  IDBDatabaseMetadata();
  IDBDatabaseMetadata(const String& name,
                      int64_t id,
                      int64_t version,
                      int64_t max_object_store_id);

  explicit IDBDatabaseMetadata(const WebIDBMetadata&);

  // Overwrites the database metadata, but does not change the object store and
  // index metadata.
  void CopyFrom(const IDBDatabaseMetadata&);

  String name;
  int64_t id;
  int64_t version;
  int64_t max_object_store_id;
  HashMap<int64_t, RefPtr<IDBObjectStoreMetadata>> object_stores;
};

}  // namespace blink

#endif  // IDBMetadata_h
