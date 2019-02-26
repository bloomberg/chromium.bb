// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/shared_proto_database.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {

namespace {

const std::string kDefaultNamespace = "ns";
const std::string kDefaultNamespace2 = "ns2";
const std::string kDefaultTypePrefix = "tp";

}  // namespace

class SharedProtoDatabaseClientTest : public testing::Test {
 public:
  void SetUp() override {
    temp_dir_.reset(new base::ScopedTempDir());
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
    db_ = base::WrapRefCounted(new SharedProtoDatabase(
        scoped_task_environment_.GetMainThreadTaskRunner(), "client",
        temp_dir_->GetPath()));
  }

  void TearDown() override {
    db_.reset();
    temp_dir_.reset();
  }

  scoped_refptr<SharedProtoDatabase> db() { return db_; }
  base::ScopedTempDir* temp_dir() { return temp_dir_.get(); }

  LevelDB* GetLevelDB() const { return db_->GetLevelDBForTesting(); }

  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClient(
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      ProtoLevelDBWrapper::InitCallback callback) {
    return db_->GetClient<T>(
        client_namespace, type_prefix, create_if_missing,
        base::BindOnce([](ProtoLevelDBWrapper::InitCallback callback,
                          bool success) { std::move(callback).Run(success); },
                       std::move(callback)));
  }

  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClientAndWait(
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      bool* success) {
    base::RunLoop loop;
    auto client = GetClient<T>(
        client_namespace, type_prefix, create_if_missing,
        base::BindOnce(
            [](bool* success_out, base::OnceClosure closure, bool success) {
              *success_out = success;
              std::move(closure).Run();
            },
            success, loop.QuitClosure()));
    loop.Run();
    return client;
  }

  bool ContainsKeys(const leveldb_proto::KeyVector& db_keys,
                    const leveldb_proto::KeyVector& keys,
                    const std::string& client_namespace,
                    const std::string& prefix) {
    std::set<std::string> key_set(db_keys.begin(), db_keys.end());
    for (auto& key : keys) {
      auto full_key =
          client_namespace.empty() || prefix.empty()
              ? key
              : base::JoinString({client_namespace, prefix, key}, "_");
      if (key_set.find(full_key) == key_set.end())
        return false;
    }
    return true;
  }

  bool ContainsEntries(const leveldb_proto::KeyVector& db_keys,
                       const std::vector<TestProto>& entries,
                       const std::string& client_namespace,
                       const std::string& type_prefix) {
    std::set<std::string> entry_id_set;
    for (auto& entry : entries)
      entry_id_set.insert(entry.id());

    // Entry IDs don't include the full prefix, so we don't look for that here.
    auto prefix =
        base::JoinString({client_namespace, type_prefix, std::string()}, "_");
    for (auto& key : db_keys) {
      if (entry_id_set.find(prefix + key) == entry_id_set.end())
        return false;
    }
    return true;
  }

  void UpdateEntries(SharedProtoDatabaseClient<TestProto>* client,
                     const leveldb_proto::KeyVector& keys,
                     const leveldb_proto::KeyVector& keys_to_remove,
                     bool expect_success) {
    auto entries = std::make_unique<ProtoDatabase<TestProto>::KeyEntryVector>();
    for (auto& key : keys) {
      TestProto proto;
      proto.set_id(client->prefix_ + key);
      entries->emplace_back(std::make_pair(key, std::move(proto)));
    }

    auto entries_to_remove = std::make_unique<leveldb_proto::KeyVector>();
    for (auto& key : keys_to_remove)
      entries_to_remove->push_back(key);

    base::RunLoop update_entries_loop;
    client->UpdateEntries(
        std::move(entries), std::move(entries_to_remove),
        base::BindOnce(
            [](base::OnceClosure signal, bool expect_success, bool success) {
              ASSERT_EQ(success, expect_success);
              std::move(signal).Run();
            },
            update_entries_loop.QuitClosure(), expect_success));
    update_entries_loop.Run();
  }

  void UpdateEntriesWithRemoveFilter(
      SharedProtoDatabaseClient<TestProto>* client,
      const leveldb_proto::KeyVector& keys,
      const LevelDB::KeyFilter& delete_key_filter,
      bool expect_success) {
    auto entries = std::make_unique<ProtoDatabase<TestProto>::KeyEntryVector>();
    for (auto& key : keys) {
      TestProto proto;
      proto.set_id(client->prefix_ + key);
      entries->emplace_back(std::make_pair(key, std::move(proto)));
    }

    base::RunLoop update_entries_loop;
    client->UpdateEntriesWithRemoveFilter(
        std::move(entries), delete_key_filter,
        base::BindOnce(
            [](base::OnceClosure signal, bool expect_success, bool success) {
              ASSERT_EQ(success, expect_success);
              std::move(signal).Run();
            },
            update_entries_loop.QuitClosure(), expect_success));
    update_entries_loop.Run();
  }

  // This fills in for all the LoadEntriesX functions because they all call this
  // one.
  void LoadEntriesWithFilter(SharedProtoDatabaseClient<TestProto>* client,
                             const LevelDB::KeyFilter& key_filter,
                             const leveldb::ReadOptions& options,
                             const std::string& target_prefix,
                             bool expect_success,
                             std::unique_ptr<std::vector<TestProto>>* entries) {
    base::RunLoop load_entries_loop;
    client->LoadEntriesWithFilter(
        key_filter, options, target_prefix,
        base::BindOnce(
            [](std::unique_ptr<std::vector<TestProto>>* entries_ptr,
               base::OnceClosure signal, bool expect_success, bool success,
               std::unique_ptr<std::vector<TestProto>> entries) {
              ASSERT_EQ(success, expect_success);
              entries_ptr->reset(entries.release());
              std::move(signal).Run();
            },
            entries, load_entries_loop.QuitClosure(), expect_success));
    load_entries_loop.Run();
  }

  void LoadKeys(SharedProtoDatabaseClient<TestProto>* client,
                bool expect_success,
                std::unique_ptr<std::vector<std::string>>* keys) {
    base::RunLoop load_keys_loop;
    client->LoadKeys(base::BindOnce(
        [](std::unique_ptr<std::vector<std::string>>* keys_ptr,
           base::OnceClosure signal, bool expect_success, bool success,
           std::unique_ptr<std::vector<std::string>> keys) {
          ASSERT_EQ(success, expect_success);
          keys_ptr->reset(keys.release());
          std::move(signal).Run();
        },
        keys, load_keys_loop.QuitClosure(), expect_success));
    load_keys_loop.Run();
  }

  std::unique_ptr<TestProto> GetEntry(
      SharedProtoDatabaseClient<TestProto>* client,
      const std::string& key,
      bool expect_success) {
    std::unique_ptr<TestProto> entry;
    base::RunLoop get_key_loop;
    client->GetEntry(
        key, base::BindOnce(
                 [](std::unique_ptr<TestProto>* entry_ptr,
                    base::OnceClosure signal, bool expect_success, bool success,
                    std::unique_ptr<TestProto> entry) {
                   ASSERT_EQ(success, expect_success);
                   entry_ptr->reset(entry.release());
                   std::move(signal).Run();
                 },
                 &entry, get_key_loop.QuitClosure(), expect_success));
    get_key_loop.Run();
    return entry;
  }

  void Destroy(SharedProtoDatabaseClient<TestProto>* client,
               bool expect_success) {
    base::RunLoop destroy_loop;
    client->Destroy(base::BindOnce(
        [](bool expect_success, base::OnceClosure signal, bool success) {
          ASSERT_EQ(success, expect_success);
          std::move(signal).Run();
        },
        expect_success, destroy_loop.QuitClosure()));
    destroy_loop.Run();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<SharedProtoDatabase> db_;
  std::unique_ptr<base::ScopedTempDir> temp_dir_;
};

TEST_F(SharedProtoDatabaseClientTest, InitSuccess) {
  bool success = false;
  auto client =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  client->Init("client",
               base::BindOnce([](bool success) { ASSERT_TRUE(success); }));
}

TEST_F(SharedProtoDatabaseClientTest, InitFail) {
  bool success = false;
  auto client =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  false /* create_if_missing */, &success);
  ASSERT_FALSE(success);

  client->Init("client",
               base::BindOnce([](bool success) { ASSERT_FALSE(success); }));
}

// Ensure that our LevelDB contains the properly prefixed entries and also
// removes prefixed entries correctly.
TEST_F(SharedProtoDatabaseClientTest, UpdateEntriesAppropriatePrefix) {
  bool success = false;
  auto client =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  std::vector<std::string> key_list = {"entry1", "entry2", "entry3"};
  UpdateEntries(client.get(), key_list, leveldb_proto::KeyVector(), true);

  // Make sure those entries are in the LevelDB with the appropriate prefix.
  std::vector<std::string> keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 3U);
  ASSERT_TRUE(
      ContainsKeys(keys, key_list, kDefaultNamespace, kDefaultTypePrefix));

  auto remove_keys = {key_list[0]};
  // Now try to delete one entry.
  UpdateEntries(client.get(), leveldb_proto::KeyVector(), remove_keys, true);

  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 2U);
  ASSERT_TRUE(ContainsKeys(keys, {key_list[1], key_list[2]}, kDefaultNamespace,
                           kDefaultTypePrefix));
}

TEST_F(SharedProtoDatabaseClientTest,
       UpdateEntries_DeletesCorrectClientEntries) {
  bool success = false;
  auto client_a =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);
  auto client_b =
      GetClientAndWait<TestProto>(kDefaultNamespace2, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  std::vector<std::string> key_list = {"entry1", "entry2", "entry3"};
  UpdateEntries(client_a.get(), key_list, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_b.get(), key_list, leveldb_proto::KeyVector(), true);

  std::vector<std::string> keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 6U);
  ASSERT_TRUE(
      ContainsKeys(keys, key_list, kDefaultNamespace, kDefaultTypePrefix));
  ASSERT_TRUE(
      ContainsKeys(keys, key_list, kDefaultNamespace2, kDefaultTypePrefix));

  // Now delete from client_b and ensure only client A's values exist.
  UpdateEntries(client_b.get(), leveldb_proto::KeyVector(), key_list, true);
  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 3U);
  ASSERT_TRUE(
      ContainsKeys(keys, key_list, kDefaultNamespace, kDefaultTypePrefix));
}

TEST_F(SharedProtoDatabaseClientTest,
       UpdateEntriesWithRemoveFilter_DeletesCorrectEntries) {
  bool success = false;
  auto client =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  std::vector<std::string> key_list = {"entry1", "entry2", "testentry3"};
  UpdateEntries(client.get(), key_list, leveldb_proto::KeyVector(), true);

  std::vector<std::string> keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 3U);

  UpdateEntriesWithRemoveFilter(client.get(), leveldb_proto::KeyVector(),
                                base::BindRepeating([](const std::string& key) {
                                  return base::StartsWith(
                                      key, "test",
                                      base::CompareCase::INSENSITIVE_ASCII);
                                }),
                                true);

  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 2U);
  // Make sure "testentry3" was removed.
  ASSERT_TRUE(ContainsKeys(keys, {"entry1", "entry2"}, kDefaultNamespace,
                           kDefaultTypePrefix));
}

TEST_F(SharedProtoDatabaseClientTest, LoadEntriesWithFilter) {
  bool success = false;
  auto client_a =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);
  auto client_b =
      GetClientAndWait<TestProto>(kDefaultNamespace2, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  std::vector<std::string> key_list_a = {"entry123", "entry2123", "testentry3"};
  UpdateEntries(client_a.get(), key_list_a, leveldb_proto::KeyVector(), true);
  std::vector<std::string> key_list_b = {"testentry124", "entry2124",
                                         "testentry4"};
  UpdateEntries(client_b.get(), key_list_b, leveldb_proto::KeyVector(), true);

  std::unique_ptr<std::vector<TestProto>> entries;
  LoadEntriesWithFilter(client_a.get(), leveldb_proto::LevelDB::KeyFilter(),
                        leveldb::ReadOptions(), std::string(), true, &entries);
  ASSERT_EQ(entries->size(), 3U);
  ASSERT_TRUE(ContainsEntries(key_list_a, *entries, kDefaultNamespace,
                              kDefaultTypePrefix));

  LoadEntriesWithFilter(client_b.get(), leveldb_proto::LevelDB::KeyFilter(),
                        leveldb::ReadOptions(), std::string(), true, &entries);
  ASSERT_EQ(entries->size(), 3U);
  ASSERT_TRUE(ContainsEntries(key_list_b, *entries, kDefaultNamespace2,
                              kDefaultTypePrefix));

  // Now test the actual filtering functionality.
  LoadEntriesWithFilter(
      client_b.get(), base::BindRepeating([](const std::string& key) {
        // Strips the entries that start with "test" from |key_list_b|.
        return !base::StartsWith(key, "test",
                                 base::CompareCase::INSENSITIVE_ASCII);
      }),
      leveldb::ReadOptions(), std::string(), true, &entries);
  ASSERT_EQ(entries->size(), 1U);
  ASSERT_TRUE(ContainsEntries({"entry2124"}, *entries, kDefaultNamespace2,
                              kDefaultTypePrefix));
}

TEST_F(SharedProtoDatabaseClientTest, LoadKeys) {
  bool success = false;
  auto client_a =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);
  auto client_b =
      GetClientAndWait<TestProto>(kDefaultNamespace2, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  std::vector<std::string> key_list_a = {"entry123", "entry2123", "testentry3",
                                         "testing"};
  UpdateEntries(client_a.get(), key_list_a, leveldb_proto::KeyVector(), true);
  std::vector<std::string> key_list_b = {"testentry124", "entry2124",
                                         "testentry4"};
  UpdateEntries(client_b.get(), key_list_b, leveldb_proto::KeyVector(), true);

  std::unique_ptr<std::vector<std::string>> keys;
  LoadKeys(client_a.get(), true, &keys);
  ASSERT_EQ(keys->size(), 4U);
  ASSERT_TRUE(ContainsKeys(*keys, key_list_a, std::string(), std::string()));
  LoadKeys(client_b.get(), true, &keys);
  ASSERT_EQ(keys->size(), 3U);
  ASSERT_TRUE(ContainsKeys(*keys, key_list_b, std::string(), std::string()));
}

TEST_F(SharedProtoDatabaseClientTest, GetEntry) {
  bool success = false;
  auto client_a =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);
  auto client_b =
      GetClientAndWait<TestProto>(kDefaultNamespace2, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  std::vector<std::string> key_list = {"a", "b", "c"};
  // Add the same entries to both because we want to make sure we only get the
  // entries from the proper client.
  UpdateEntries(client_a.get(), key_list, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_b.get(), key_list, leveldb_proto::KeyVector(), true);

  auto a_prefix = base::JoinString(
      {kDefaultNamespace, kDefaultTypePrefix, std::string()}, "_");
  auto b_prefix = base::JoinString(
      {kDefaultNamespace2, kDefaultTypePrefix, std::string()}, "_");

  for (auto& key : key_list) {
    auto entry = GetEntry(client_a.get(), key, true);
    ASSERT_EQ(entry->id(), a_prefix + key);
    entry = GetEntry(client_b.get(), key, true);
    ASSERT_EQ(entry->id(), b_prefix + key);
  }
}

TEST_F(SharedProtoDatabaseClientTest, TestDestroy) {
  bool success = false;
  auto client_a =
      GetClientAndWait<TestProto>(kDefaultNamespace, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);
  auto client_b =
      GetClientAndWait<TestProto>(kDefaultNamespace2, kDefaultTypePrefix,
                                  true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  std::vector<std::string> key_list = {"a", "b", "c"};
  // Add the same entries to both because we want to make sure we only destroy
  // the entries from the proper client.
  UpdateEntries(client_a.get(), key_list, leveldb_proto::KeyVector(), true);
  UpdateEntries(client_b.get(), key_list, leveldb_proto::KeyVector(), true);

  // Delete only client A.
  std::vector<std::string> keys;
  LevelDB* db = GetLevelDB();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), key_list.size() * 2);
  ASSERT_TRUE(
      ContainsKeys(keys, key_list, kDefaultNamespace, kDefaultTypePrefix));
  ASSERT_TRUE(
      ContainsKeys(keys, key_list, kDefaultNamespace2, kDefaultTypePrefix));
  Destroy(client_a.get(), true);

  // Make sure only client B remains and delete client B.
  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), key_list.size());
  ASSERT_TRUE(
      ContainsKeys(keys, key_list, kDefaultNamespace2, kDefaultTypePrefix));
  Destroy(client_b.get(), true);

  // Nothing should remain.
  keys.clear();
  db->LoadKeys(&keys);
  ASSERT_EQ(keys.size(), 0U);
}

}  // namespace leveldb_proto