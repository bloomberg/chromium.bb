// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "webkit/dom_storage/session_storage_database.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
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
                             std::string* namespace_id);
  static bool IsNamespaceOriginKey(const std::string& key,
                                   std::string* namespace_id);
  static bool IsMapRefCountKey(const std::string& key,
                               int64* map_id);
  static bool IsMapValueKey(const std::string& key,
                            int64* map_id);
  void ResetDatabase();
  void ReadData(DataMap* data) const;
  void CheckDatabaseConsistency() const;
  void CheckEmptyDatabase() const;
  void DumpData() const;
  void CheckAreaData(const std::string& namespace_id,
                     const GURL& origin,
                     const ValuesMap& reference) const;
  void CompareValuesMaps(const ValuesMap& map1, const ValuesMap& map2) const;
  void CheckNamespaceIds(
      const std::set<std::string>& expected_namespace_ids) const;
  void CheckOrigins(
      const std::string& namespace_id,
      const std::set<GURL>& expected_origins) const;
  std::string GetMapForArea(const std::string& namespace_id,
                            const GURL& origin) const;
  int64 GetMapRefCount(const std::string& map_id) const;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<SessionStorageDatabase> db_;

  // Test data.
  const GURL kOrigin1;
  const GURL kOrigin2;
  const std::string kNamespace1;
  const std::string kNamespace2;
  const std::string kNamespaceClone;
  const base::string16 kKey1;
  const base::string16 kKey2;
  const base::string16 kKey3;
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
      kNamespace1("namespace1"),
      kNamespace2("namespace2"),
      kNamespaceClone("wascloned"),
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
                                                std::string* namespace_id) {
  std::string namespace_prefix = SessionStorageDatabase::NamespacePrefix();
  if (key.find(namespace_prefix) != 0)
    return false;
  if (key == namespace_prefix)
    return false;

  size_t second_dash = key.find('-', namespace_prefix.length());
  if (second_dash != key.length() - 1)
    return false;

  // Key is of the form "namespace-<namespaceid>-".
  *namespace_id = key.substr(
      namespace_prefix.length(),
      second_dash - namespace_prefix.length());
  return true;
}

// static
bool SessionStorageDatabaseTest::IsNamespaceOriginKey(
    const std::string& key,
    std::string* namespace_id) {
  std::string namespace_prefix = SessionStorageDatabase::NamespacePrefix();
  if (key.find(namespace_prefix) != 0)
    return false;
  size_t second_dash = key.find('-', namespace_prefix.length());
  if (second_dash == std::string::npos || second_dash == key.length() - 1)
    return false;

  // Key is of the form "namespace-<namespaceid>-<origin>", and the value
  // is the map id.
  *namespace_id = key.substr(
      namespace_prefix.length(),
      second_dash - namespace_prefix.length());
  return true;
}

// static
bool SessionStorageDatabaseTest::IsMapRefCountKey(const std::string& key,
                                                  int64* map_id) {
  std::string map_prefix = "map-";
  if (key.find(map_prefix) != 0)
    return false;
  size_t second_dash = key.find('-', map_prefix.length());
  if (second_dash != key.length() - 1)
    return false;
  // Key is of the form "map-<mapid>-" and the value is the ref count.
  std::string map_id_str = key.substr(map_prefix.length(),
                                      second_dash - map_prefix.length());
  bool conversion_ok = base::StringToInt64(map_id_str, map_id);
  EXPECT_TRUE(conversion_ok);
  return true;
}

// static
bool SessionStorageDatabaseTest::IsMapValueKey(const std::string& key,
                                               int64* map_id) {
  std::string map_prefix = "map-";
  if (key.find(map_prefix) != 0)
    return false;
  size_t second_dash = key.find('-', map_prefix.length());
  if (second_dash == std::string::npos || second_dash == key.length() - 1)
    return false;
  // Key is of the form "map-<mapid>-key".
  std::string map_id_str = key.substr(map_prefix.length(),
                                      second_dash - map_prefix.length());
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

  std::string next_map_id_key = SessionStorageDatabase::NextMapIdKey();
  // Check the namespace start key.
  if (data.find(SessionStorageDatabase::NamespacePrefix()) == data.end()) {
    // If there is no namespace start key, the database may contain only counter
    // keys.
    for (DataMap::const_iterator it = data.begin(); it != data.end(); ++it) {
      ASSERT_TRUE(it->first == next_map_id_key);
    }
    return;
  }
  ++valid_keys;

  // Iterate the "namespace-" keys.
  std::set<std::string> found_namespace_ids;
  std::set<std::string> namespaces_with_areas;
  std::map<int64, int64> expected_map_refcounts;
  int64 max_map_id = -1;

  for (DataMap::const_iterator it = data.begin(); it != data.end(); ++it) {
    std::string namespace_id;
    std::string origin;
    if (IsNamespaceKey(it->first, &namespace_id)) {
      found_namespace_ids.insert(namespace_id);
      ++valid_keys;
    } else if (IsNamespaceOriginKey(
        it->first, &namespace_id)) {
      // Check that the corresponding "namespace-<namespaceid>-" key exists. It
      // has been read by now, since the keys are stored in order.
      ASSERT_TRUE(found_namespace_ids.find(namespace_id) !=
                  found_namespace_ids.end());
      namespaces_with_areas.insert(namespace_id);
      int64 map_id;
      bool conversion_ok = base::StringToInt64(it->second, &map_id);
      ASSERT_TRUE(conversion_ok);
      ASSERT_GE(map_id, 0);
      ++expected_map_refcounts[map_id];
      max_map_id = std::max(map_id, max_map_id);
      ++valid_keys;
    }
  }
  // Check that there are no leftover "namespace-namespaceid-" keys without
  // associated areas.
  ASSERT_EQ(found_namespace_ids.size(), namespaces_with_areas.size());

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
      // Convert the value back to base::string16.
      base::string16 value;
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
    const std::string& namespace_id, const GURL& origin,
    const ValuesMap& reference) const {
  ValuesMap values;
  db_->ReadAreaValues(namespace_id, origin, &values);
  CompareValuesMaps(values, reference);
}

void SessionStorageDatabaseTest::CompareValuesMaps(
    const ValuesMap& map1,
    const ValuesMap& map2) const {
  ASSERT_EQ(map2.size(), map1.size());
  for (ValuesMap::const_iterator it = map1.begin(); it != map1.end(); ++it) {
    base::string16 key = it->first;
    ASSERT_TRUE(map2.find(key) != map2.end());
    NullableString16 val1 = it->second;
    NullableString16 val2 = map2.find(key)->second;
    EXPECT_EQ(val2.is_null(), val1.is_null());
    EXPECT_EQ(val2.string(), val1.string());
  }
}

void SessionStorageDatabaseTest::CheckNamespaceIds(
    const std::set<std::string>& expected_namespace_ids) const {
  std::map<std::string, std::vector<GURL> > namespaces_and_origins;
  EXPECT_TRUE(db_->ReadNamespacesAndOrigins(&namespaces_and_origins));
  EXPECT_EQ(expected_namespace_ids.size(), namespaces_and_origins.size());
  for (std::map<std::string, std::vector<GURL> >::const_iterator it =
           namespaces_and_origins.begin();
       it != namespaces_and_origins.end(); ++it) {
    EXPECT_TRUE(expected_namespace_ids.find(it->first) !=
                expected_namespace_ids.end());
  }
}

void SessionStorageDatabaseTest::CheckOrigins(
    const std::string& namespace_id,
    const std::set<GURL>& expected_origins) const {
  std::map<std::string, std::vector<GURL> > namespaces_and_origins;
  EXPECT_TRUE(db_->ReadNamespacesAndOrigins(&namespaces_and_origins));
  const std::vector<GURL>& origins = namespaces_and_origins[namespace_id];
  EXPECT_EQ(expected_origins.size(), origins.size());
  for (std::vector<GURL>::const_iterator it = origins.begin();
       it != origins.end(); ++it) {
    EXPECT_TRUE(expected_origins.find(*it) != expected_origins.end());
  }
}

std::string SessionStorageDatabaseTest::GetMapForArea(
    const std::string& namespace_id, const GURL& origin) const {
  bool exists;
  std::string map_id;
  EXPECT_TRUE(db_->GetMapForArea(namespace_id, origin.spec(),
                                 leveldb::ReadOptions(), &exists, &map_id));
  EXPECT_TRUE(exists);
  return map_id;
}

int64 SessionStorageDatabaseTest::GetMapRefCount(
    const std::string& map_id) const {
  int64 ref_count;
  EXPECT_TRUE(db_->GetMapRefCount(map_id, &ref_count));
  return ref_count;
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
    EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, changes));
  }
  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, reference);

  // Overwrite and delete values.
  {
    ValuesMap changes;
    changes[kKey1] = kValue4;
    changes[kKey3] = kValueNull;
    reference[kKey1] = kValue4;
    reference.erase(kKey3);
    EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, changes));
  }
  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, reference);

  // Clear data before writing.
  {
    ValuesMap changes;
    changes[kKey2] = kValue2;
    reference.erase(kKey1);
    reference[kKey2] = kValue2;
    EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, true, changes));
  }
  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, reference);
}

TEST_F(SessionStorageDatabaseTest, WriteDataForTwoOrigins) {
  // Write data.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));

  ValuesMap data2;
  data2[kKey1] = kValue4;
  data2[kKey2] = kValue1;
  data2[kKey3] = kValue2;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));

  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, data1);
  CheckAreaData(kNamespace1, kOrigin2, data2);
}

TEST_F(SessionStorageDatabaseTest, WriteDataForTwoNamespaces) {
  // Write data.
  ValuesMap data11;
  data11[kKey1] = kValue1;
  data11[kKey2] = kValue2;
  data11[kKey3] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data11));
  ValuesMap data12;
  data12[kKey2] = kValue4;
  data12[kKey3] = kValue3;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data12));
  ValuesMap data21;
  data21[kKey1] = kValue2;
  data21[kKey2] = kValue4;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespace2, kOrigin1, false, data21));
  ValuesMap data22;
  data22[kKey2] = kValue1;
  data22[kKey3] = kValue2;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespace2, kOrigin2, false, data22));
  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, data11);
  CheckAreaData(kNamespace1, kOrigin2, data12);
  CheckAreaData(kNamespace2, kOrigin1, data21);
  CheckAreaData(kNamespace2, kOrigin2, data22);
}

TEST_F(SessionStorageDatabaseTest, ShallowCopy) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));
  // Make a shallow copy.
  EXPECT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));
  // Now both namespaces should have the same data.
  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, data1);
  CheckAreaData(kNamespace1, kOrigin2, data2);
  CheckAreaData(kNamespaceClone, kOrigin1, data1);
  CheckAreaData(kNamespaceClone, kOrigin2, data2);
  // Both the namespaces refer to the same maps.
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin1),
            GetMapForArea(kNamespaceClone, kOrigin1));
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin2),
            GetMapForArea(kNamespaceClone, kOrigin2));
  EXPECT_EQ(2, GetMapRefCount(GetMapForArea(kNamespace1, kOrigin1)));
  EXPECT_EQ(2, GetMapRefCount(GetMapForArea(kNamespace1, kOrigin2)));
}

TEST_F(SessionStorageDatabaseTest, WriteIntoShallowCopy) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  EXPECT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));

  // Write data into a shallow copy.
  ValuesMap changes;
  ValuesMap reference;
  changes[kKey1] = kValueNull;
  changes[kKey2] = kValue4;
  changes[kKey3] = kValue4;
  reference[kKey2] = kValue4;
  reference[kKey3] = kValue4;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespaceClone, kOrigin1, false,
                                     changes));

  // Values in the original namespace were not changed.
  CheckAreaData(kNamespace1, kOrigin1, data1);
  // But values in the copy were.
  CheckAreaData(kNamespaceClone, kOrigin1, reference);

  // The namespaces no longer refer to the same map.
  EXPECT_NE(GetMapForArea(kNamespace1, kOrigin1),
            GetMapForArea(kNamespaceClone, kOrigin1));
  EXPECT_EQ(1, GetMapRefCount(GetMapForArea(kNamespace1, kOrigin1)));
  EXPECT_EQ(1, GetMapRefCount(GetMapForArea(kNamespaceClone, kOrigin1)));
}

TEST_F(SessionStorageDatabaseTest, ManyShallowCopies) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));

  // Make a two shallow copies.
  EXPECT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));
  std::string another_clone("another_cloned");
  EXPECT_TRUE(db_->CloneNamespace(kNamespace1, another_clone));

  // Make a shallow copy of a shallow copy.
  std::string clone_of_clone("clone_of_clone");
  EXPECT_TRUE(db_->CloneNamespace(another_clone, clone_of_clone));

  // Now all namespaces should have the same data.
  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, data1);
  CheckAreaData(kNamespaceClone, kOrigin1, data1);
  CheckAreaData(another_clone, kOrigin1, data1);
  CheckAreaData(clone_of_clone, kOrigin1, data1);
  CheckAreaData(kNamespace1, kOrigin2, data2);
  CheckAreaData(kNamespaceClone, kOrigin2, data2);
  CheckAreaData(another_clone, kOrigin2, data2);
  CheckAreaData(clone_of_clone, kOrigin2, data2);

  // All namespaces refer to the same maps.
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin1),
            GetMapForArea(kNamespaceClone, kOrigin1));
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin2),
            GetMapForArea(kNamespaceClone, kOrigin2));
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin1),
            GetMapForArea(another_clone, kOrigin1));
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin2),
            GetMapForArea(another_clone, kOrigin2));
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin1),
            GetMapForArea(clone_of_clone, kOrigin1));
  EXPECT_EQ(GetMapForArea(kNamespace1, kOrigin2),
            GetMapForArea(clone_of_clone, kOrigin2));

  // Check the ref counts.
  EXPECT_EQ(4, GetMapRefCount(GetMapForArea(kNamespace1, kOrigin1)));
  EXPECT_EQ(4, GetMapRefCount(GetMapForArea(kNamespace1, kOrigin2)));
}

TEST_F(SessionStorageDatabaseTest, DisassociateShallowCopy) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  EXPECT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));

  // Disassoaciate the shallow copy.
  EXPECT_TRUE(db_->DeleteArea(kNamespaceClone, kOrigin1));
  CheckDatabaseConsistency();

  // Now new data can be written to that map.
  ValuesMap reference;
  ValuesMap changes;
  changes[kKey1] = kValueNull;
  changes[kKey2] = kValue4;
  changes[kKey3] = kValue4;
  reference[kKey2] = kValue4;
  reference[kKey3] = kValue4;
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespaceClone, kOrigin1, false,
                                     changes));

  // Values in the original map were not changed.
  CheckAreaData(kNamespace1, kOrigin1, data1);

  // But values in the disassociated map were.
  CheckAreaData(kNamespaceClone, kOrigin1, reference);
}

TEST_F(SessionStorageDatabaseTest, DeleteNamespace) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey2] = kValue4;
  data2[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));
  EXPECT_TRUE(db_->DeleteNamespace(kNamespace1));
  CheckDatabaseConsistency();
  CheckEmptyDatabase();
}

TEST_F(SessionStorageDatabaseTest, DeleteNamespaceWithShallowCopy) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));

  // Make a shallow copy and delete the original namespace.
  EXPECT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));
  EXPECT_TRUE(db_->DeleteNamespace(kNamespace1));

  // The original namespace has no data.
  CheckDatabaseConsistency();
  CheckAreaData(kNamespace1, kOrigin1, ValuesMap());
  CheckAreaData(kNamespace1, kOrigin2, ValuesMap());
  // But the copy persists.
  CheckAreaData(kNamespaceClone, kOrigin1, data1);
  CheckAreaData(kNamespaceClone, kOrigin2, data2);
}

TEST_F(SessionStorageDatabaseTest, DeleteArea) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));

  EXPECT_TRUE(db_->DeleteArea(kNamespace1, kOrigin2));
  CheckDatabaseConsistency();
  // The data for the non-deleted origin persists.
  CheckAreaData(kNamespace1, kOrigin1, data1);
  // The data for the deleted origin is gone.
  CheckAreaData(kNamespace1, kOrigin2, ValuesMap());
}

TEST_F(SessionStorageDatabaseTest, DeleteAreaWithShallowCopy) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  data2[kKey3] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));

  // Make a shallow copy and delete an origin from the original namespace.
  EXPECT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));
  EXPECT_TRUE(db_->DeleteArea(kNamespace1, kOrigin1));
  CheckDatabaseConsistency();

  // The original namespace has data for only the non-deleted origin.
  CheckAreaData(kNamespace1, kOrigin1, ValuesMap());
  CheckAreaData(kNamespace1, kOrigin2, data2);
  // But the copy persists.
  CheckAreaData(kNamespaceClone, kOrigin1, data1);
  CheckAreaData(kNamespaceClone, kOrigin2, data2);
}

TEST_F(SessionStorageDatabaseTest, WriteRawBytes) {
  // Write data which is not valid utf8 and contains null bytes.
  unsigned char raw_data[10] = {255, 0, 0, 0, 1, 2, 3, 4, 5, 0};
  ValuesMap changes;
  base::string16 string_with_raw_data;
  string_with_raw_data.assign(reinterpret_cast<char16*>(raw_data), 5);
  changes[kKey1] = NullableString16(string_with_raw_data, false);
  EXPECT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, changes));
  CheckDatabaseConsistency();
  ValuesMap values;
  db_->ReadAreaValues(kNamespace1, kOrigin1, &values);
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(values[kKey1].string().data());
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(raw_data[i], data[i]);
}

TEST_F(SessionStorageDatabaseTest, DeleteNamespaceConfusion) {
  // Regression test for a bug where a namespace with id 10 prevented deleting
  // the namespace with id 1.

  ValuesMap data1;
  data1[kKey1] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges("foobar", kOrigin1, false, data1));
  ASSERT_TRUE(db_->CommitAreaChanges("foobarbaz", kOrigin1, false, data1));

  // Delete the namespace with ID 1.
  EXPECT_TRUE(db_->DeleteNamespace("foobar"));
}

TEST_F(SessionStorageDatabaseTest, ReadNamespaceIds) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;
  std::set<std::string> expected_namespace_ids;

  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  expected_namespace_ids.insert(kNamespace1);
  CheckNamespaceIds(expected_namespace_ids);

  ASSERT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));
  expected_namespace_ids.insert(kNamespaceClone);
  CheckNamespaceIds(expected_namespace_ids);

  ASSERT_TRUE(db_->DeleteNamespace(kNamespace1));
  expected_namespace_ids.erase(kNamespace1);
  CheckNamespaceIds(expected_namespace_ids);

  CheckDatabaseConsistency();
}

TEST_F(SessionStorageDatabaseTest, ReadNamespaceIdsInEmptyDatabase) {
  std::set<std::string> expected_namespace_ids;
  CheckNamespaceIds(expected_namespace_ids);
}

TEST_F(SessionStorageDatabaseTest, ReadOriginsInNamespace) {
  ValuesMap data1;
  data1[kKey1] = kValue1;
  data1[kKey2] = kValue2;
  data1[kKey3] = kValue3;

  std::set<GURL> expected_origins1;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data1));
  expected_origins1.insert(kOrigin1);
  expected_origins1.insert(kOrigin2);
  CheckOrigins(kNamespace1, expected_origins1);

  std::set<GURL> expected_origins2;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace2, kOrigin2, false, data1));
  expected_origins2.insert(kOrigin2);
  CheckOrigins(kNamespace2, expected_origins2);

  ASSERT_TRUE(db_->CloneNamespace(kNamespace1, kNamespaceClone));
  CheckOrigins(kNamespaceClone, expected_origins1);

  ASSERT_TRUE(db_->DeleteArea(kNamespace1, kOrigin2));
  expected_origins1.erase(kOrigin2);
  CheckOrigins(kNamespace1, expected_origins1);

  CheckDatabaseConsistency();
}

TEST_F(SessionStorageDatabaseTest, DeleteAllOrigins) {
  // Write data for a namespace, for 2 origins.
  ValuesMap data1;
  data1[kKey1] = kValue1;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin1, false, data1));
  ValuesMap data2;
  data2[kKey1] = kValue2;
  ASSERT_TRUE(db_->CommitAreaChanges(kNamespace1, kOrigin2, false, data2));

  EXPECT_TRUE(db_->DeleteArea(kNamespace1, kOrigin1));
  EXPECT_TRUE(db_->DeleteArea(kNamespace1, kOrigin2));
  // Check that also the namespace start key was deleted.
  CheckDatabaseConsistency();
}


}  // namespace dom_storage
