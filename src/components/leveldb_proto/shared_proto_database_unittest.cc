// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {

namespace {

const std::string kDefaultNamespace = "ns";
const std::string kDefaultNamespace2 = "ns2";
const std::string kDefaultTypePrefix = "tp";

}  // namespace

class SharedProtoDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    temp_dir_ = std::make_unique<base::ScopedTempDir>();
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
    db_thread_ = std::make_unique<base::Thread>("db_thread");
    ASSERT_TRUE(db_thread_->Start());
    db_ = base::WrapRefCounted(new SharedProtoDatabase(
        db_thread_->task_runner(), "client", temp_dir_->GetPath()));
  }

  void TearDown() override {}

  void InitDB(bool create_if_missing,
              ProtoLevelDBWrapper::InitCallback callback) {
    db_->Init(create_if_missing, std::move(callback),
              scoped_task_environment_.GetMainThreadTaskRunner());
  }

  void KillDB() { db_.reset(); }

  scoped_refptr<SharedProtoDatabase> CreateDatabase(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const char* client_name,
      const base::FilePath& db_dir) {
    return base::WrapRefCounted(
        new SharedProtoDatabase(task_runner, client_name, db_dir));
  }

  bool IsDatabaseInitialized(SharedProtoDatabase* db) {
    return db->init_state_ == SharedProtoDatabase::InitState::kSuccess;
  }

  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClientAndWait(
      SharedProtoDatabase* db,
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      bool* success) {
    base::RunLoop loop;
    auto client = db->GetClient<T>(
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

  scoped_refptr<base::SequencedTaskRunner> GetMainThreadTaskRunner() {
    return scoped_task_environment_.GetMainThreadTaskRunner();
  }

  SharedProtoDatabase* db() { return db_.get(); }
  ProtoLevelDBWrapper* wrapper() { return db_->db_wrapper_.get(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  std::unique_ptr<base::Thread> db_thread_;
  scoped_refptr<SharedProtoDatabase> db_;
};

inline void GetClientFromTaskRunner(SharedProtoDatabase* db,
                                    const std::string& client_namespace,
                                    const std::string& type_prefix,
                                    base::OnceClosure closure) {
  db->GetClient<TestProto>(
      client_namespace, type_prefix, true /* create_if_missing */,
      base::BindOnce([](base::OnceClosure closure,
                        bool success) { std::move(closure).Run(); },
                     std::move(closure)));
}

TEST_F(SharedProtoDatabaseTest, CreateClient_SucceedsWithCreate) {
  bool success = false;
  GetClientAndWait<TestProto>(db(), kDefaultNamespace, kDefaultTypePrefix,
                              true /* create_if_missing */, &success);
  ASSERT_TRUE(success);
}

TEST_F(SharedProtoDatabaseTest, CreateClient_FailsWithoutCreate) {
  bool success = false;
  GetClientAndWait<TestProto>(db(), kDefaultNamespace, kDefaultTypePrefix,
                              false /* create_if_missing */, &success);
  ASSERT_FALSE(success);
}

TEST_F(SharedProtoDatabaseTest,
       CreateClient_SucceedsWithoutCreateIfAlreadyCreated) {
  bool success = false;
  GetClientAndWait<TestProto>(db(), kDefaultNamespace2, kDefaultTypePrefix,
                              true /* create_if_missing */, &success);
  ASSERT_TRUE(success);
  GetClientAndWait<TestProto>(db(), kDefaultNamespace, kDefaultTypePrefix,
                              false /* create_if_missing */, &success);
  ASSERT_TRUE(success);
}

TEST_F(SharedProtoDatabaseTest, GetClient_DifferentThreads) {
  bool success = false;
  GetClientAndWait<TestProto>(db(), kDefaultNamespace, kDefaultTypePrefix,
                              true /* create_if_missing */, &success);
  ASSERT_TRUE(success);

  base::Thread t("test_thread");
  ASSERT_TRUE(t.Start());
  base::RunLoop run_loop;
  t.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GetClientFromTaskRunner,
                                base::Unretained(db()), kDefaultNamespace2,
                                kDefaultTypePrefix, run_loop.QuitClosure()));
  run_loop.Run();
  base::RunLoop quit_cooldown;
  GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, quit_cooldown.QuitClosure(), base::TimeDelta::FromSeconds(3));
}

// Tests that the shared DB's destructor behaves appropriately once the
// backing LevelDB has been initialized on another thread.
TEST_F(SharedProtoDatabaseTest, TestDBDestructionAfterInit) {
  base::RunLoop run_init_loop;
  InitDB(true /* create_if_missing */,
         base::BindOnce(
             [](base::OnceClosure signal, bool success) {
               ASSERT_TRUE(success);
               std::move(signal).Run();
             },
             run_init_loop.QuitClosure()));
  run_init_loop.Run();
  KillDB();
}

}  // namespace leveldb_proto