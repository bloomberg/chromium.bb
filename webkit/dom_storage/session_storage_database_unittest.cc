// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/session_storage_database.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "webkit/dom_storage/dom_storage_types.h"

namespace dom_storage {

class SessionStorageDatabaseTest : public testing::Test {
 public:
  SessionStorageDatabaseTest();
  virtual ~SessionStorageDatabaseTest();
  virtual void SetUp() OVERRIDE;

 protected:
  typedef std::map<std::string, std::string> DataMap;

  // Helpers.
  static bool IsNamespaceKey(const std::string& key,
                             int64* namespace_id);
  static bool IsNamespaceOriginKey(const std::string& key,
                                   int64* namespace_id);
  static bool IsMapRefCountKey(const std::string& key,
                               int64* map_id);
  static bool IsMapValueKey(const std::string& key,
                            int64* map_id);
  void ResetDatabase();
  void ReadData(DataMap* data) const;
  void CheckDatabaseConsistency() const;
  void CheckEmptyDatabase() const;
  void DumpData() const;
  void CheckAreaData(int64 namespace_id,
                     const GURL& origin,
                     const ValuesMap& reference) const;
  void CompareValuesMaps(const ValuesMap& map1, const ValuesMap& map2) const;
  std::string GetMapForArea(int64 namespace_id,
                            const GURL& origin) const;
  int64 GetMapRefCount(const std::string& map_id) const;
  int64 NextNamespaceId() const;

  ScopedTempDir temp_dir_;
  scoped_refptr<SessionStorageDatabase> db_;

  // Test data.
  const GURL kOrigin1;
  const GURL kOrigin2;
  const string16 kKey1;
  const string16 kKey2;
  const string16 kKey3;
  const NullableString16 kValue1;
  const NullableString16 kValue2;
  const NullableString16 kValue3;
  const NullableString16 kValue4;
  const NullableString16 kValueNull;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageDatabaseTest);
};

SessionStorageDatabaseTest::SessionStorageDatabaseTest()
    : kOrigin1("http://www.origin1.com"),
      kOrigin2("http://www.origin2.com"),
      kKey1(ASCIIToUTF16("key1")),
      kKey2(ASCIIToUTF16("key2")),
      kKey3(ASCIIToUTF16("key3")),
      kValue1(NullableString16(ASCIIToUTF16("value1"), false)),
      kValue2(NullableString16(ASCIIToUTF16("value2"), false)),
      kValue3(NullableString16(ASCIIToUTF16("value3"), false)),
      kValue4(NullableString16(ASCIIToUTF16("value4"), false)),
      kValueNull(NullableString16(true)) { }

SessionStorageDatabaseTest::~SessionStorageDatabaseTest() { }

void SessionStorageDatabaseTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ResetDatabase();
}

void SessionStorageDatabaseTest::ResetDatabase() {
  db_ = new SessionStorageDatabase(temp_dir_.path());
  ASSERT_TRUE(db_->LazyOpen(true));
}

// static
bool SessionStorageDatabaseTest::IsNamespaceKey(const std::string& key,
                                                int64* namespace_id) {
  std::string namespace_prefix = SessionStorageDatabase::NamespacePrefix();
  if (key.find(namespace_prefix) != 0)
    return false;
  if (key == namespace_prefix)
    return false;

  size_t second_dash = key.find('-', namespace_prefix.length());
  if (second_dash != std::string::npos)
    return false;

  // Key is of the form "namespace-<namespaceid>".
  std::string namespace_id_str = key.substr(namespace_prefix.length());
  bool conversion_ok = base::StringToInt64(namespace_id_str, namespace_id);
  EXPECT_TRUE(conversion_ok);
  return true;
}

// static
bool SessionStorageDatabaseTest::IsNamespaceOriginKey(const std::string& key,
                                                      int64* namespace_id) {
  std::string namespace_prefix = SessionStorageDatabase::NamespacePrefix();
  if (key.find(namespace_prefix) != 0)
    return false;
  size_t second_dash = key.find('-', namespace_prefix.length());
  if (second_dash == std::string::npos)
    return false;
  // Key is of the form "namespace-<namespaceid>-<origin>", and the value
  // is the map id.
  std::string namespace_id_str =
      key.substr(namespace_prefix.length(),
                 second_dash - namespace_prefix.length());
  bool conversion_ok = base::StringToInt64(namespace_id_str, namespace_id);
  EXPECT_TRUE(conversion_ok);
  return true;
}

// static
bool SessionStorageDatabaseTest::IsMapRefCountKey(const std::string& key,
                                                  int64* map_id) {
  std::string map_prefix = SessionStorageDatabase::MapPrefix();
  if (key.find(map_prefix) != 0)
    return false;
  size_t second_dash = key.find('-', map_prefix.length());
  if (second_dash != std::string::npos)
    return false;
  // Key is of the form "map-<mapid>" and the value is the ref count.
  std::string map_id_str = key.substr(map_prefix.length(), second_dash);
  bool conversion_ok = base::StringToInt64(map_id_str, map_id);
  EXPECT_TRUE(conversion_ok);
  return true;
}

// static
bool SessionStorageDatabaseTest::IsMapValueKey(const std::string& key,
                                               int64* map_id) {
  std::string map_prefix = SessionStorageDatabase::MapPrefix();
  if (key.find(map_prefix) != 0)
    return false;
  size_t second_dash = key.find('-', map_prefix.length());
  if (second_dash == std::string::npos)
    return false;
  // Key is of the form "map-<mapid>-key".
  std::string map_id_str =
      key.substr(map_prefix.length(), second_dash - map_prefix.length());
  bool conversion_ok = base::StringToInt64(map_id_str, map_id);
  EXPECT_TRUE(conversion_ok);
  return true;
}

void SessionStorageDatabaseTest::ReadData(DataMap* data) const {
  leveldb::DB* leveldb = db_->db_.get();
  scoped_ptr<leveldb::Iterator> it(
      leveldb->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    (*data)[it->key().ToString()] = it->value().ToString();
  }
}

void SessionStorageDatabaseTest::CheckDatabaseConsistency() const {
  DataMap data;
  ReadData(&data);
  // Empty db is ok.
  if (data.empty())
    return;

  // For detecting rubbish keys.
  size_t valid_keys = 0;

  std::string next_namespace_id_key =
      SessionStorageDatabase::NextNamespaceIdKey();
  std::string next_map_id_key = SessionStorageDatabase::NextMapIdKey();
  // Check the namespace start key.
  if (data.find(SessionStorageDatabase::NamespacePrefix()) == data.end()) {
    // If there is no namespace start key, the database may contain only counter
    // keys.
    for (DataMap::const_iterator it = data.begin(); it != data.end(); ++it) {
      ASSERT_TRUE(it->first == next_namespace_id_key ||
                  it->first == next_map_id_key);
    }
    return;
  }
  ++valid_keys;

  // Iterate the "namespace-" keys.
  std::set<int64> found_namespace_ids;
  int64 max_namespace_id = -1;
  std::map<int64, int64> expected_map_refcounts;
  int64 max_map_id = -1;

  for (DataMap::const_iterator it = data.begin(); it != data.end(); ++it) {
    int64 namespace_id;
    std::string origin;
    if (IsNamespaceKey(it->first, &namespace_id)) {
      ASSERT_GT(namespace_id, 0);
      found_namespace_ids.insert(namespace_id);
      max_namespace_id = std::max(namespace_id, max_namespace_id);
      ++valid_keys;
    } else if (IsNamespaceOriginKey(
        it->first, &namespace_id)) {
      // Check that the corresponding "namespace-<namespaceid>" key exists. It
      // has been read by now, since the keys are stored in order.
      ASSERT_TRUE(found_namespace_ids.find(namespace_id) !=
                  found_namespace_ids.end());
      int64 map_id;
      bool conversion_ok = base::StringToInt64(it->second, &map_id);
      ASSERT_TRUE(conversion_ok);
      ASSERT_GE(map_id, 0);
      ++expected_map_refcounts[map_id];
      max_map_id = std::max(map_id, max_map_id);
      ++valid_keys;
    }
  }
  if (max_namespace_id != -1) {
    // The database contains namespaces.
    ASSERT_TRUE(data.find(next_namespace_id_key) != data.end());
    int64 next_namespace_id;
    bool conversion_ok =
        base::StringToInt64(data[next_namespace_id_key], &next_namespace_id);
    ASSERT_TRUE(conversion_ok);
    ASSERT_GT(next_namespace_id, max_namespace_id);
  }
  if (max_map_id != -1) {
    // The database contains maps.
    ASSERT_TRUE(data.find(next_map_id_key) != data.end());
    int64 next_map_id;
    bool conversion_ok =
        base::StringToInt64(data[next_map_id_key], &next_map_id);
    ASSERT_TRUE(conversion_ok);
    ASSERT_GT(next_map_id, max_map_id);
  }

  // Iterate the "map-" keys.
  std::set<int64> found_map_ids;
  for (DataMap::const_iterator it = data.begin(); it != data.end(); ++it) {
    int64 map_id;
    if (IsMapRefCountKey(it->first, &map_id)) {
      int64 ref_count;
      bool conversion_ok = base::StringToInt64(it->second, &ref_count);
      ASSERT_TRUE(conversion_ok);
      // Check that the map is not stale.
      ASSERT_GT(ref_count, 0);
      ASSERT_TRUE(expected_map_refcounts.find(map_id) !=
                  expected_map_refcounts.end());
      ASSERT_EQ(expected_map_refcounts[map_id], ref_count);
      // Mark the map as existing.
      expected_map_refcounts.erase(map_id);
      found_map_ids.insert(map_id);
      ++valid_keys;
    } else if (IsMapValueKey(it->first, &map_id)) {
      ASSERT_TRUE(found_map_ids.find(map_id) != found_map_ids.end());
      ++valid_keys;
    }
  }
  // Check that all maps referred to exist.
  ASSERT_TRUE(expected_map_refcounts.empty());

  // Count valid keys.
  if (data.find(next_namespace_id_key) != data.end())
    ++valid_keys;

  if (data.find(next_map_id_key) != data.end())
    ++valid_keys;

  ASSERT_EQ(data.size(), valid_keys);
}

void SessionStorageDatabaseTest::CheckEmptyDatabase() const {
  DataMap data;
  ReadData(&data);
  size_t valid_keys = 0;
  if (data.find(SessionStorageDatabase::NamespacePrefix()) != data.end())
    ++valid_keys;
  if (data.find(SessionStorageDatabase::NextNamespaceIdKey()) != data.end())
    ++valid_keys;
  if (data.find(SessionStorageDatabase::NextMapIdKey()) != data.end())
    ++valid_keys;
  EXPECT_EQ(valid_keys, data.size());
}

void SessionStorageDatabaseTest::DumpData() const {
  LOG(WARNING) << "---- Session storage contents";
  scoped_ptr<leveldb::Iterator> it(
      db_->db_->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    int64 dummy_map_id;
    if (IsMapValueKey(it->key().ToString(), &dummy_map_id)) {
      // Convert the value back to string16.
      string16 value;
      size_t len = it->value().size() / sizeof(char16);
      value.resize(len);
      value.assign(reinterpret_cast<const char16*>(it->value().data()), len);
      LOG(WARNING) << it->key().ToString() << ": " << value;
    } else {
      LOG(WARNING) << it->key().ToString() << ": " << it->value().ToString();
    }
  }
  LOG(WARNING) << "----";
}

void SessionStorageDatabaseTest::CheckAreaData(
    int64 namespace_id, const GURL& origin, const ValuesMap& reference) const {
  ValuesMap values;
  db_->ReadAreaValues(namespace_id, origin, &values);
  CompareValuesMaps(values, reference);
}

void SessionStorageDatabaseTest::CompareValuesMaps(
    const ValuesMap& map1,
    const ValuesMap& map2) const {
  ASSERT_EQ(map2.size(), map1.size());
  for (ValuesMap::const_iterator it = map1.begin(); it != map1.end(); ++it) {
    string16 key = it->first;
    ASSERT_TRUE(map2.find(key) != map2.end());
    NullableString16 val1 = it->second;
    NullableString16 val2 = map2.find(key)->second;
    EXPECT_EQ(val2.is_null(), val1.is_null());
    EXPECT_EQ(val2.string(), val1.string());
  }
}

std::string SessionStorageDatabaseTest::GetMapForArea(
    int64 namespace_id, const GURL& origin) const {
  bool exists;
  std::string map_id;
  EXPECT_TRUE(db_->GetMapForArea(namespace_id, origin,
                                 &exists, &map_id));
  EXPECT_TRUE(exists);
  return map_id;
}

int64 SessionStorageDatabaseTest::GetMapRefCount(
    const std::string& map_id) const {
  int64 ref_count;
  EXPECT_TRUE(db_->GetMapRefCount(map_id, &ref_count));
  return ref_count;
}

int64 SessionStorageDatabaseTest::NextNamespaceId() const {
  int64 next_namespace_id;
  EXPECT_TRUE(db_->GetNextNamespaceId(&next_namespace_id));
  return next_namespace_id;
}

TEST_F(SessionStorageDatabaseTest, EmptyDatabaseSanityCheck) {
  // An empty database should be valid.
  CheckDatabaseConsistency();
}

TEST_F(SessionStorageDatabaseTest, WriteDataForOneOrigin) {
  // Keep track on what the values should look like.
  ValuesMap reference;
  // Write data.
  {
    ValuesMap changes;
    changes[kKey1] = kValue1;
    changes[kKey2] = kValue2;
    changes[kKey3] = kValue3;
    reference[kKey1] = kValue1;
    reference[kKey2] = kValue2;
    reference[kKey3] = kValue3;
    EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, changes));
  }
  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, reference);

  // Overwrite and delete values.
  {
    ValuesMap changes;
    changes[kKey1] = kValue4;
    changes[kKey3] = kValueNull;
    reference[kKey1] = kValue4;
    reference.erase(kKey3);
    EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, changes));
  }
  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, reference);

  // Clear data before writing.
  {
    ValuesMap changes;
    changes[kKey2] = kValue2;
    reference.erase(kKey1);
    reference[kKey2] = kValue2;
    EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, true, changes));
  }
  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, reference);
}

TEST_F(SessionStorageDatabaseTest, WriteDataForTwoOrigins) {
  // Write data.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));

  ValuesMap data2;
  data2[kKey1] = kValue4;
  data2[kKey2] = kValue1;
  data2[kKey3] = kValue2;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data2));

  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, data1);
  CheckAreaData(1, kOrigin2, data2);
}

TEST_F(SessionStorageDatabaseTest, WriteDataForTwoNamespaces) {
  // Write data.
  ValuesMap data11;
  data11[kKey1] = kValue1;
  data11[kKey2] = kValue2;
  data11[kKey3] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data11));
  ValuesMap data12;
  data12[kKey2] = kValue4;
  data12[kKey3] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data12));
  ValuesMap data21;
  data21[kKey1] = kValue2;
  data21[kKey2] = kValue4;
  EXPECT_TRUE(db_->CommitAreaChanges(2, kOrigin1, false, data21));
  ValuesMap data22;
  data22[kKey2] = kValue1;
  data22[kKey3] = kValue2;
  EXPECT_TRUE(db_->CommitAreaChanges(2, kOrigin2, false, data22));
  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, data11);
  CheckAreaData(1, kOrigin2, data12);
  CheckAreaData(2, kOrigin1, data21);
  CheckAreaData(2, kOrigin2, data22);
}

TEST_F(SessionStorageDatabaseTest, ShallowCopy) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data2));
  // Make a shallow copy.
  EXPECT_TRUE(db_->CloneNamespace(1, 5));
  // Now both namespaces should have the same data.
  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, data1);
  CheckAreaData(1, kOrigin2, data2);
  CheckAreaData(5, kOrigin1, data1);
  CheckAreaData(5, kOrigin2, data2);
  // Both the namespaces refer to the same maps.
  EXPECT_EQ(GetMapForArea(1, kOrigin1), GetMapForArea(5, kOrigin1));
  EXPECT_EQ(GetMapForArea(1, kOrigin2), GetMapForArea(5, kOrigin2));
  EXPECT_EQ(2, GetMapRefCount(GetMapForArea(1, kOrigin1)));
  EXPECT_EQ(2, GetMapRefCount(GetMapForArea(1, kOrigin2)));
}

TEST_F(SessionStorageDatabaseTest, WriteIntoShallowCopy) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  EXPECT_TRUE(db_->CloneNamespace(1, 5));

  // Write data into a shallow copy.
  ValuesMap changes;
  ValuesMap reference;
  changes[kKey1] = kValueNull;
  changes[kKey2] = kValue4;
  changes[kKey3] = kValue4;
  reference[kKey2] = kValue4;
  reference[kKey3] = kValue4;
  EXPECT_TRUE(db_->CommitAreaChanges(5, kOrigin1, false, changes));

  // Values in the original namespace were not changed.
  CheckAreaData(1, kOrigin1, data1);
  // But values in the copy were.
  CheckAreaData(5, kOrigin1, reference);

  // The namespaces no longer refer to the same map.
  EXPECT_NE(GetMapForArea(1, kOrigin1), GetMapForArea(5, kOrigin1));
  EXPECT_EQ(1, GetMapRefCount(GetMapForArea(1, kOrigin1)));
  EXPECT_EQ(1, GetMapRefCount(GetMapForArea(5, kOrigin1)));
}

TEST_F(SessionStorageDatabaseTest, ManyShallowCopies) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data2));

  // Make a two shallow copies.
  EXPECT_TRUE(db_->CloneNamespace(1, 5));
  EXPECT_TRUE(db_->CloneNamespace(1, 6));

  // Make a shallow copy of a shallow copy.
  EXPECT_TRUE(db_->CloneNamespace(6, 7));

  // Now all namespaces should have the same data.
  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, data1);
  CheckAreaData(5, kOrigin1, data1);
  CheckAreaData(6, kOrigin1, data1);
  CheckAreaData(7, kOrigin1, data1);
  CheckAreaData(1, kOrigin2, data2);
  CheckAreaData(5, kOrigin2, data2);
  CheckAreaData(6, kOrigin2, data2);
  CheckAreaData(7, kOrigin2, data2);

  // All namespaces refer to the same maps.
  EXPECT_EQ(GetMapForArea(1, kOrigin1), GetMapForArea(5, kOrigin1));
  EXPECT_EQ(GetMapForArea(1, kOrigin2), GetMapForArea(5, kOrigin2));
  EXPECT_EQ(GetMapForArea(1, kOrigin1), GetMapForArea(6, kOrigin1));
  EXPECT_EQ(GetMapForArea(1, kOrigin2), GetMapForArea(6, kOrigin2));
  EXPECT_EQ(GetMapForArea(1, kOrigin1), GetMapForArea(7, kOrigin1));
  EXPECT_EQ(GetMapForArea(1, kOrigin2), GetMapForArea(7, kOrigin2));

  // Check the ref counts.
  EXPECT_EQ(4, GetMapRefCount(GetMapForArea(1, kOrigin1)));
  EXPECT_EQ(4, GetMapRefCount(GetMapForArea(1, kOrigin2)));
}

TEST_F(SessionStorageDatabaseTest, DisassociateShallowCopy) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  EXPECT_TRUE(db_->CloneNamespace(1, 5));

  // Disassoaciate the shallow copy.
  EXPECT_TRUE(db_->DeleteArea(5, kOrigin1));
  CheckDatabaseConsistency();

  // Now new data can be written to that map.
  ValuesMap reference;
  ValuesMap changes;
  changes[kKey1] = kValueNull;
  changes[kKey2] = kValue4;
  changes[kKey3] = kValue4;
  reference[kKey2] = kValue4;
  reference[kKey3] = kValue4;
  EXPECT_TRUE(db_->CommitAreaChanges(5, kOrigin1, false, changes));

  // Values in the original map were not changed.
  CheckAreaData(1, kOrigin1, data1);

  // But values in the disassociated map were.
  CheckAreaData(5, kOrigin1, reference);
}

TEST_F(SessionStorageDatabaseTest, DeleteNamespace) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey2] = kValue4;
  data2[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data2));
  EXPECT_TRUE(db_->DeleteNamespace(1));
  CheckDatabaseConsistency();
  CheckEmptyDatabase();
}

TEST_F(SessionStorageDatabaseTest, DeleteNamespaceWithShallowCopy) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data2));

  // Make a shallow copy and delete the original namespace.
  EXPECT_TRUE(db_->CloneNamespace(1, 5));;
  EXPECT_TRUE(db_->DeleteNamespace(1));

  // The original namespace has no data.
  CheckDatabaseConsistency();
  CheckAreaData(1, kOrigin1, ValuesMap());
  CheckAreaData(1, kOrigin2, ValuesMap());
  // But the copy persists.
  CheckAreaData(5, kOrigin1, data1);
  CheckAreaData(5, kOrigin2, data2);
}

TEST_F(SessionStorageDatabaseTest, DeleteArea) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data2));

  EXPECT_TRUE(db_->DeleteArea(1, kOrigin2));
  CheckDatabaseConsistency();
  // The data for the non-deleted origin persists.
  CheckAreaData(1, kOrigin1, data1);
  // The data for the deleted origin is gone.
  CheckAreaData(1, kOrigin2, ValuesMap());
}

TEST_F(SessionStorageDatabaseTest, DeleteAreaWithShallowCopy) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin2, false, data2));

  // Make a shallow copy and delete an origin from the original namespace.
  EXPECT_TRUE(db_->CloneNamespace(1, 5));
  EXPECT_TRUE(db_->DeleteArea(1, kOrigin1));
  CheckDatabaseConsistency();

  // The original namespace has data for only the non-deleted origin.
  CheckAreaData(1, kOrigin1, ValuesMap());
  CheckAreaData(1, kOrigin2, data2);
  // But the copy persists.
  CheckAreaData(5, kOrigin1, data1);
  CheckAreaData(5, kOrigin2, data2);
}

TEST_F(SessionStorageDatabaseTest, WriteRawBytes) {
  // Write data which is not valid utf8 and contains null bytes.
  unsigned char raw_data[10] = {255, 0, 0, 0, 1, 2, 3, 4, 5, 0};
  ValuesMap changes;
  string16 string_with_raw_data;
  string_with_raw_data.assign(reinterpret_cast<char16*>(raw_data), 5);
  changes[kKey1] = NullableString16(string_with_raw_data, false);
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, changes));
  CheckDatabaseConsistency();
  ValuesMap values;
  db_->ReadAreaValues(1, kOrigin1, &values);
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(values[kKey1].string().data());
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(raw_data[i], data[i]);
}

TEST_F(SessionStorageDatabaseTest, NextNamespaceId) {
  // Create namespaces, check the next namespace id.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(10, kOrigin1, false, data1));
  EXPECT_EQ(10 + 1, NextNamespaceId());
  ASSERT_TRUE(db_->CommitAreaChanges(343, kOrigin1, false, data1));
  EXPECT_EQ(343 + 1, NextNamespaceId());
  ASSERT_TRUE(db_->CommitAreaChanges(99, kOrigin1, false, data1));
  EXPECT_EQ(343 + 1, NextNamespaceId());

  // Close the database and recreate it.
  ResetDatabase();

  // The next namespace id is persisted.
  EXPECT_EQ(344, NextNamespaceId());

  // Create more namespaces.
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));
  EXPECT_EQ(344 + 1 + 1, NextNamespaceId());

  EXPECT_TRUE(db_->CommitAreaChanges(959, kOrigin1, false, data1));
  EXPECT_EQ(344 + 959 + 1, NextNamespaceId());
}

TEST_F(SessionStorageDatabaseTest, NamespaceOffset) {
  // Create a namespace with id 1.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));

  // Close the database and recreate it.
  ResetDatabase();

  // Create another namespace with id 1.
  ValuesMap data2;
  data2[kKey1] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data2));

  // Now the values for namespace 1 are the new ones.
  CheckAreaData(1, kOrigin1, data2);

  // The values for the old namespace 1 are still accessible via id -1.
  CheckAreaData(-1, kOrigin1, data1);
}

TEST_F(SessionStorageDatabaseTest, NamespaceOffsetCloneNamespace) {
  // Create a namespace with id 1.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));

  // Close the database and recreate it.
  ResetDatabase();

  // Create another namespace with id 1.
  ValuesMap data2;
  data2[kKey1] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data2));

  // Make a shallow copy of the newly created namespace.
  EXPECT_TRUE(db_->CloneNamespace(1, 20));

  // The clone contains values from the newly created namespace.
  CheckAreaData(20, kOrigin1, data2);
  CheckAreaData(1, kOrigin1, data2);

  // The values for the old namespace 1 are still accessible via id -1.
  CheckAreaData(-1, kOrigin1, data1);

  // Close the database and recreate it.
  ResetDatabase();

  // The namespace and the clone are still accessible.
  CheckAreaData(-1, kOrigin1, data2);
  CheckAreaData(-20, kOrigin1, data2);
  CheckAreaData(-22, kOrigin1, data1);
}

TEST_F(SessionStorageDatabaseTest, NamespaceOffsetWriteIntoShallowCopy) {
  // Create a namespace with id 1.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));

  // Close the database and recreate it.
  ResetDatabase();

  // Create another namespace with id 1.
  ValuesMap data2;
  data2[kKey1] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data2));

  // Make a shallow copy of the newly created namespace.
  EXPECT_TRUE(db_->CloneNamespace(1, 20));

  // Now the values can be altered and a deep copy will be made.
  ValuesMap data3;
  data3[kKey1] = kValue2;
  EXPECT_TRUE(db_->CommitAreaChanges(20, kOrigin1, false, data3));

  CheckAreaData(20, kOrigin1, data3);
  CheckAreaData(1, kOrigin1, data2);

  // The values for the old namespace 1 are still accessible via id -1.
  CheckAreaData(-1, kOrigin1, data1);

  // Close the database and recreate it.
  ResetDatabase();

  // The namespace and the deep copy are still accessible.
  CheckAreaData(-1, kOrigin1, data3);
  CheckAreaData(-20, kOrigin1, data2);
  CheckAreaData(-22, kOrigin1, data1);
}

TEST_F(SessionStorageDatabaseTest, NamespaceOffsetDeleteArea) {
  // Create a namespace with id 1.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));

  // Close the database and recreate it.
  ResetDatabase();

  // Create another namespace with id 1.
  ValuesMap data2;
  data2[kKey1] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data2));

  // Delete kOrigin1 from the newly created namespace.
  EXPECT_TRUE(db_->DeleteArea(1, kOrigin1));

  // Namespace 1 is empty.
  CheckAreaData(1, kOrigin1, ValuesMap());

  // The values for the old namespace 1 are still accessible via id -1.
  CheckAreaData(-1, kOrigin1, data1);
}

TEST_F(SessionStorageDatabaseTest, NamespaceOffsetDeleteNamespace) {
  // Create a namespace with id 1.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data1));

  // Close the database and recreate it.
  ResetDatabase();

  // Create another namespace with id 1.
  ValuesMap data2;
  data2[kKey1] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(1, kOrigin1, false, data2));

  // Delete the newly created namespace.
  EXPECT_TRUE(db_->DeleteNamespace(1));

  // Namespace 1 is empty.
  CheckAreaData(1, kOrigin1, ValuesMap());

  // The values for the old namespace 1 are still accessible via id -1.
  CheckAreaData(-1, kOrigin1, data1);
}

}  // namespace dom_storage
