// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/session_storage_database.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

// Layout of the database:
// | key                            | value                              |
// -----------------------------------------------------------------------
// | map-1                          | 2 (refcount, start of map-1-* keys)|
// | map-1-a                        | b (a = b in map 1)                 |
// | ...                            |                                    |
// | namespace-                     | dummy (start of namespace-* keys)  |
// | namespace-1 (1 = namespace id) | dummy (start of namespace-1-* keys)|
// | namespace-1-origin1            | 1 (mapid)                          |
// | namespace-1-origin2            | 2                                  |
// | namespace-2                    | dummy                              |
// | namespace-2-origin1            | 1 (shallow copy)                   |
// | namespace-2-origin2            | 2 (shallow copy)                   |
// | namespace-3                    | dummy                              |
// | namespace-3-origin1            | 3 (deep copy)                      |
// | namespace-3-origin2            | 2 (shallow copy)                   |
// | next-namespace-id              | 4                                  |
// | next-map-id                    | 4                                  |

namespace dom_storage {

SessionStorageDatabase::SessionStorageDatabase(const FilePath& file_path)
    : file_path_(file_path),
      db_error_(false),
      is_inconsistent_(false),
      namespace_offset_(0) {
}

SessionStorageDatabase::~SessionStorageDatabase() {
}

void SessionStorageDatabase::ReadAreaValues(int64 namespace_id,
                                            const GURL& origin,
                                            ValuesMap* result) {
  // We don't create a database if it doesn't exist. In that case, there is
  // nothing to be added to the result.
  if (!LazyOpen(false))
    return;
  std::string map_id;
  bool exists;
  if (!GetMapForArea(namespace_id, origin, &exists, &map_id))
    return;
  if (exists)
    ReadMap(map_id, result, false);
}

bool SessionStorageDatabase::CommitAreaChanges(int64 namespace_id,
                                               const GURL& origin,
                                               bool clear_all_first,
                                               const ValuesMap& changes) {
  // Even if |changes| is empty, we need to write the appropriate placeholders
  // in the database, so that it can be later shallow-copied succssfully.
  if (!LazyOpen(true))
    return false;

  leveldb::WriteBatch batch;
  // Ensure that the keys "namespace-" "namespace-N" (see the schema above)
  // exist.
  const bool kOkIfExists = true;
  if (!CreateNamespace(namespace_id, kOkIfExists, &batch))
    return false;

  std::string map_id;
  bool exists;
  if (!GetMapForArea(namespace_id, origin, &exists, &map_id))
    return false;
  if (exists) {
    int64 ref_count;
    if (!GetMapRefCount(map_id, &ref_count))
      return false;
    if (ref_count > 1) {
      if (!DeepCopyArea(namespace_id, origin, !clear_all_first,
                        &map_id, &batch))
        return false;
    }
    else if (clear_all_first) {
      if (!ClearMap(map_id, &batch))
        return false;
    }
  } else {
    // Map doesn't exist, create it now if needed.
    if (!changes.empty()) {
      if (!CreateMapForArea(namespace_id, origin, &map_id, &batch))
        return false;
    }
  }

  WriteValuesToMap(map_id, changes, &batch);

  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::CloneNamespace(int64 namespace_id,
                                            int64 new_namespace_id) {
  // Go through all origins in the namespace |namespace_id|, create placeholders
  // for them in |new_namespace_id|, and associate them with the existing maps.

  // Example, data before shallow copy:
  // | map-1                          | 1 (refcount)        |
  // | map-1-a                        | b                   |
  // | namespace-1 (1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |

  // Example, data after shallow copy:
  // | map-1                          | 2 (inc. refcount)   |
  // | map-1-a                        | b                   |
  // | namespace-1 (1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2                    | dummy               |
  // | namespace-2-origin1            | 1 (mapid) << references the same map

  if (!LazyOpen(true))
    return false;

  leveldb::WriteBatch batch;
  const bool kOkIfExists = false;
  if (!CreateNamespace(new_namespace_id, kOkIfExists, &batch))
    return false;

  std::map<std::string, std::string> areas;
  if (!GetAreasInNamespace(namespace_id, &areas))
    return false;

  for (std::map<std::string, std::string>::const_iterator it = areas.begin();
       it != areas.end(); ++it) {
    const std::string& origin = it->first;
    const std::string& map_id = it->second;
    if (!IncreaseMapRefCount(map_id, &batch))
      return false;
    AddAreaToNamespace(new_namespace_id, origin, map_id, &batch);
  }
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::DeleteArea(int64 namespace_id,
                                        const GURL& origin) {
  if (!LazyOpen(false)) {
    // No need to create the database if it doesn't exist.
    return true;
  }
  leveldb::WriteBatch batch;
  if (!DeleteArea(namespace_id, origin.spec(), &batch))
    return false;
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::DeleteNamespace(int64 namespace_id) {
  if (!LazyOpen(false)) {
    // No need to create the database if it doesn't exist.
    return true;
  }
  // Itereate through the areas in the namespace.
  leveldb::WriteBatch batch;
  std::map<std::string, std::string> areas;
  if (!GetAreasInNamespace(namespace_id, &areas))
    return false;
  for (std::map<std::string, std::string>::const_iterator it = areas.begin();
       it != areas.end(); ++it) {
    const std::string& origin = it->first;
    if (!DeleteArea(namespace_id, origin, &batch))
      return false;
  }
  batch.Delete(NamespaceStartKey(namespace_id, namespace_offset_));
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::LazyOpen(bool create_if_needed) {
  base::AutoLock auto_lock(db_lock_);
  if (db_error_ || is_inconsistent_) {
    // Don't try to open a database that we know has failed already.
    return false;
  }
  if (IsOpen())
    return true;

  if (!create_if_needed &&
      (!file_util::PathExists(file_path_) ||
       file_util::IsDirectoryEmpty(file_path_))) {
    // If the directory doesn't exist already and we haven't been asked to
    // create a file on disk, then we don't bother opening the database. This
    // means we wait until we absolutely need to put something onto disk before
    // we do so.
    return false;
  }

  leveldb::DB* db;
  leveldb::Status s = TryToOpen(&db);
  if (!s.ok()) {
    LOG(WARNING) << "Failed to open leveldb in " << file_path_.value()
                 << ", error: " << s.ToString();
    DCHECK(db == NULL);

    // Clear the directory and try again.
    file_util::Delete(file_path_, true);
    s = TryToOpen(&db);
    if (!s.ok()) {
      LOG(WARNING) << "Failed to open leveldb in " << file_path_.value()
                   << ", error: " << s.ToString();
      DCHECK(db == NULL);
      db_error_ = true;
      return false;
    }
  }
  db_.reset(db);

  return GetNextNamespaceId(&namespace_offset_);
}

leveldb::Status SessionStorageDatabase::TryToOpen(leveldb::DB** db) {
  leveldb::Options options;
  // The directory exists but a valid leveldb database might not exist inside it
  // (e.g., a subset of the needed files might be missing). Handle this
  // situation gracefully by creating the database now.
  options.create_if_missing = true;
#if defined(OS_WIN)
  return leveldb::DB::Open(options, WideToUTF8(file_path_.value()), db);
#elif defined(OS_POSIX)
  return leveldb::DB::Open(options, file_path_.value(), db);
#endif
}

bool SessionStorageDatabase::IsOpen() const {
  return db_.get() != NULL;
}

bool SessionStorageDatabase::CallerErrorCheck(bool ok) const {
  DCHECK(ok);
  return ok;
}

bool SessionStorageDatabase::ConsistencyCheck(bool ok) {
  if (ok)
    return true;
  base::AutoLock auto_lock(db_lock_);
  DCHECK(false);
  is_inconsistent_ = true;
  // We cannot recover the database during this run, e.g., the upper layer can
  // have a different understanding of the database state (shallow and deep
  // copies).
  // TODO(marja): Error handling.
  return false;
}

bool SessionStorageDatabase::DatabaseErrorCheck(bool ok) {
  if (ok)
    return true;
  base::AutoLock auto_lock(db_lock_);
  db_error_ = true;
  // TODO(marja): Error handling.
  return false;
}

bool SessionStorageDatabase::CreateNamespace(int64 namespace_id,
                                             bool ok_if_exists,
                                             leveldb::WriteBatch* batch) {
  leveldb::Slice namespace_prefix = NamespacePrefix();
  std::string dummy;
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), namespace_prefix,
                               &dummy);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  if (s.IsNotFound())
    batch->Put(namespace_prefix, "");

  std::string namespace_start_key =
      NamespaceStartKey(namespace_id, namespace_offset_);
  s = db_->Get(leveldb::ReadOptions(), namespace_start_key, &dummy);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  if (s.IsNotFound()) {
    batch->Put(namespace_start_key, "");
    return UpdateNextNamespaceId(namespace_id, batch);
  }
  return CallerErrorCheck(ok_if_exists);
}

bool SessionStorageDatabase::GetNextNamespaceId(int64* next_namespace_id) {
  std::string next_namespace_id_string;
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), NextNamespaceIdKey(),
                               &next_namespace_id_string);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  if (s.IsNotFound()) {
    *next_namespace_id = 0;
    return true;
  }
  bool conversion_ok =
      base::StringToInt64(next_namespace_id_string, next_namespace_id);
  return ConsistencyCheck(conversion_ok);
}

bool SessionStorageDatabase::UpdateNextNamespaceId(int64 namespace_id,
                                                   leveldb::WriteBatch* batch) {
  int64 next_namespace_id;
  if (!GetNextNamespaceId(&next_namespace_id))
    return false;
  if (next_namespace_id < namespace_id + namespace_offset_ + 1) {
    next_namespace_id = namespace_id + namespace_offset_ + 1;
    batch->Put(NextNamespaceIdKey(), base::Int64ToString(next_namespace_id));
  }
  return true;
}

bool SessionStorageDatabase::GetAreasInNamespace(
    int64 namespace_id,
    std::map<std::string, std::string>* areas) {
  return GetAreasInNamespace(NamespaceIdStr(namespace_id, namespace_offset_),
                             areas);
}

bool SessionStorageDatabase::GetAreasInNamespace(
    const std::string& namespace_id_str,
    std::map<std::string, std::string>* areas) {
  std::string namespace_start_key = NamespaceStartKey(namespace_id_str);
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
  it->Seek(namespace_start_key);
  if (it->status().IsNotFound()) {
    // The namespace_start_key is not found when the namespace doesn't contain
    // any areas. We don't need to do anything.
    return true;
  }
  if (!DatabaseErrorCheck(it->status().ok()))
    return false;

  // Skip the dummy entry "namespace-<namespaceid>" and iterate the origins.
  for (it->Next(); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.find(namespace_start_key) != 0) {
      // Iterated past the origins for this namespace.
      break;
    }
    size_t second_dash = key.find('-', namespace_start_key.length());
    if (!ConsistencyCheck(second_dash != std::string::npos))
      return false;
    std::string origin = key.substr(second_dash + 1);
    std::string map_id = it->value().ToString();
    (*areas)[origin] = map_id;
  }
  return true;
}

void SessionStorageDatabase::AddAreaToNamespace(int64 namespace_id,
                                                const std::string& origin,
                                                const std::string& map_id,
                                                leveldb::WriteBatch* batch) {
  std::string namespace_key = NamespaceKey(
      NamespaceIdStr(namespace_id, namespace_offset_), origin);
  batch->Put(namespace_key, map_id);
}

bool SessionStorageDatabase::DeleteArea(int64 namespace_id,
                                        const std::string& origin,
                                        leveldb::WriteBatch* batch) {
  return DeleteArea(NamespaceIdStr(namespace_id, namespace_offset_),
                    origin, batch);
}

bool SessionStorageDatabase::DeleteArea(const std::string& namespace_id_str,
                                        const std::string& origin,
                                        leveldb::WriteBatch* batch) {
  std::string map_id;
  bool exists;
  if (!GetMapForArea(namespace_id_str, origin, &exists, &map_id))
    return false;
  if (!exists)
    return true;  // Nothing to delete.
  if (!DecreaseMapRefCount(map_id, 1, batch))
    return false;
  std::string namespace_key = NamespaceKey(namespace_id_str, origin);
  batch->Delete(namespace_key);
  return true;
}

bool SessionStorageDatabase::GetMapForArea(int64 namespace_id,
                                           const GURL& origin,
                                           bool* exists,
                                           std::string* map_id) {
  return GetMapForArea(
      base::Int64ToString(namespace_id + namespace_offset_),
      origin.spec(), exists, map_id);
}

bool SessionStorageDatabase::GetMapForArea(const std::string& namespace_id_str,
                                           const std::string& origin,
                                           bool* exists, std::string* map_id) {
  std::string namespace_key = NamespaceKey(namespace_id_str, origin);
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), namespace_key, map_id);
  if (s.IsNotFound()) {
    *exists = false;
    return true;
  }
  *exists = true;
  return DatabaseErrorCheck(s.ok());
}

bool SessionStorageDatabase::CreateMapForArea(int64 namespace_id,
                                              const GURL& origin,
                                              std::string* map_id,
                                              leveldb::WriteBatch* batch) {
  leveldb::Slice next_map_id_key = NextMapIdKey();
  leveldb::Status s = db_->Get(leveldb::ReadOptions(), next_map_id_key, map_id);
  if (!DatabaseErrorCheck(s.ok() || s.IsNotFound()))
    return false;
  int64 next_map_id = 0;
  if (s.IsNotFound()) {
    *map_id = "0";
  } else {
    bool conversion_ok = base::StringToInt64(*map_id, &next_map_id);
    if (!ConsistencyCheck(conversion_ok))
      return false;
  }
  batch->Put(next_map_id_key, base::Int64ToString(++next_map_id));
  std::string namespace_key =
      NamespaceKey(namespace_id, namespace_offset_, origin);
  batch->Put(namespace_key, *map_id);
  batch->Put(MapRefCountKey(*map_id), "1");
  return true;
}

bool SessionStorageDatabase::ReadMap(const std::string& map_id,
                                     ValuesMap* result,
                                     bool only_keys) {
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
  std::string map_start_key = MapRefCountKey(map_id);
  it->Seek(map_start_key);
  // The map needs to exist, otherwise we have a stale map_id in the database.
  if (!ConsistencyCheck(!it->status().IsNotFound()))
    return false;
  if (!DatabaseErrorCheck(it->status().ok()))
    return false;
  const int kPrefixLength = std::string(MapPrefix()).length();
  // Skip the dummy entry "map-<mapid>".
  for (it->Next(); it->Valid(); it->Next()) {
    // Key is of the form "map-<mapid>-<key>".
    std::string key = it->key().ToString();
    size_t second_dash = key.find('-', kPrefixLength);
    if (second_dash == std::string::npos ||
        key.substr(kPrefixLength, second_dash - kPrefixLength) != map_id) {
      // Iterated beyond the keys in this map.
      break;
    }
    string16 key16 = UTF8ToUTF16(key.substr(second_dash + 1));
    if (only_keys) {
      (*result)[key16] = NullableString16(true);
    } else {
      // Convert the raw data stored in std::string (it->value()) to raw data
      // stored in string16.
      size_t len = it->value().size() / sizeof(char16);
      const char16* data_ptr =
          reinterpret_cast<const char16*>(it->value().data());
      (*result)[key16] = NullableString16(string16(data_ptr, len), false);
    }
  }
  return true;
}

void SessionStorageDatabase::WriteValuesToMap(const std::string& map_id,
                                              const ValuesMap& values,
                                              leveldb::WriteBatch* batch) {
  for (ValuesMap::const_iterator it = values.begin(); it != values.end();
       ++it) {
    NullableString16 value = it->second;
    std::string key = MapKey(map_id, UTF16ToUTF8(it->first));
    if (value.is_null()) {
      batch->Delete(key);
    } else {
      // Convert the raw data stored in string16 to raw data stored in
      // std::string.
      const char* data = reinterpret_cast<const char*>(value.string().data());
      size_t size = value.string().size() * 2;
      batch->Put(key, leveldb::Slice(data, size));
    }
  }
}

bool SessionStorageDatabase::GetMapRefCount(const std::string& map_id,
                                            int64* ref_count) {
  std::string ref_count_string;
  leveldb::Status s = db_->Get(leveldb::ReadOptions(),
                               MapRefCountKey(map_id), &ref_count_string);
  if (!ConsistencyCheck(s.ok()))
    return false;
  bool conversion_ok = base::StringToInt64(ref_count_string, ref_count);
  return ConsistencyCheck(conversion_ok);
}

bool SessionStorageDatabase::IncreaseMapRefCount(const std::string& map_id,
                                                 leveldb::WriteBatch* batch) {
  // Increase the ref count for the map.
  int64 old_ref_count;
  if (!GetMapRefCount(map_id, &old_ref_count))
    return false;
  batch->Put(MapRefCountKey(map_id), base::Int64ToString(++old_ref_count));
  return true;
}

bool SessionStorageDatabase::DecreaseMapRefCount(const std::string& map_id,
                                                 int decrease,
                                                 leveldb::WriteBatch* batch) {
  // Decrease the ref count for the map.
  int64 ref_count;
  if (!GetMapRefCount(map_id, &ref_count))
    return false;
  if (!ConsistencyCheck(decrease <= ref_count))
    return false;
  ref_count -= decrease;
  if (ref_count > 0) {
    batch->Put(MapRefCountKey(map_id), base::Int64ToString(ref_count));
  } else {
    // Clear all keys in the map.
    if (!ClearMap(map_id, batch))
      return false;
    batch->Delete(MapRefCountKey(map_id));
  }
  return true;
}

bool SessionStorageDatabase::ClearMap(const std::string& map_id,
                                      leveldb::WriteBatch* batch) {
  ValuesMap values;
  if (!ReadMap(map_id, &values, true))
    return false;
  for (ValuesMap::const_iterator it = values.begin(); it != values.end(); ++it)
    batch->Delete(MapKey(map_id, UTF16ToUTF8(it->first)));
  return true;
}

bool SessionStorageDatabase::DeepCopyArea(
    int64 namespace_id, const GURL& origin, bool copy_data,
    std::string* map_id, leveldb::WriteBatch* batch) {
  // Example, data before deep copy:
  // | namespace-1 (1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2                    | dummy               |
  // | namespace-2-origin1            | 1 (mapid) << references the same map
  // | map-1                          | 2 (refcount)        |
  // | map-1-a                        | b                   |

  // Example, data after deep copy copy:
  // | namespace-1 (1 = namespace id) | dummy               |
  // | namespace-1-origin1            | 1 (mapid)           |
  // | namespace-2                    | dummy               |
  // | namespace-2-origin1            | 2 (mapid) << references the new map
  // | map-1                          | 1 (dec. refcount)   |
  // | map-1-a                        | b                   |
  // | map-2                          | 1 (refcount)        |
  // | map-2-a                        | b                   |

  // Read the values from the old map here. If we don't need to copy the data,
  // this can stay empty.
  ValuesMap values;
  if (copy_data && !ReadMap(*map_id, &values, false))
    return false;
  if (!DecreaseMapRefCount(*map_id, 1, batch))
    return false;
  // Create a new map (this will also break the association to the old map) and
  // write the old data into it. This will write the id of the created map into
  // |map_id|.
  if (!CreateMapForArea(namespace_id, origin, map_id, batch))
    return false;
  WriteValuesToMap(*map_id, values, batch);
  return true;
}

std::string SessionStorageDatabase::NamespaceStartKey(
    const std::string& namespace_id_str) {
  return base::StringPrintf("namespace-%s", namespace_id_str.c_str());
}

std::string SessionStorageDatabase::NamespaceStartKey(int64 namespace_id,
                                                      int64 namespace_offset) {
  return NamespaceStartKey(NamespaceIdStr(namespace_id, namespace_offset));
}

std::string SessionStorageDatabase::NamespaceKey(
    const std::string& namespace_id_str, const std::string& origin) {
  return base::StringPrintf("namespace-%s-%s", namespace_id_str.c_str(),
                            origin.c_str());
}

std::string SessionStorageDatabase::NamespaceKey(
    int64 namespace_id, int64 namespace_offset, const GURL& origin) {
  return NamespaceKey(NamespaceIdStr(namespace_id, namespace_offset),
                      origin.spec());
}

std::string SessionStorageDatabase::NamespaceIdStr(int64 namespace_id,
                                                   int64 namespace_offset) {
  return base::Int64ToString(namespace_id + namespace_offset);
}

const char* SessionStorageDatabase::NamespacePrefix() {
  return "namespace-";
}

std::string SessionStorageDatabase::MapRefCountKey(const std::string& map_id) {
  return base::StringPrintf("map-%s", map_id.c_str());
}

std::string SessionStorageDatabase::MapKey(const std::string& map_id,
                                           const std::string& key) {
  return base::StringPrintf("map-%s-%s", map_id.c_str(), key.c_str());
}

const char* SessionStorageDatabase::MapPrefix() {
  return "map-";
}

const char* SessionStorageDatabase::NextNamespaceIdKey() {
  return "next-namespace-id";
}

const char* SessionStorageDatabase::NextMapIdKey() {
  return "next-map-id";
}

}  // namespace dom_storage
