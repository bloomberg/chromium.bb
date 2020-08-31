// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_impl.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/legacy_dom_storage_database.h"
#include "components/services/storage/dom_storage/storage_area_test_util.h"
#include "components/services/storage/dom_storage/testing_legacy_session_storage_database.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> StringPieceToUint8Vector(base::StringPiece s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> String16ToUint8Vector(const base::string16& s) {
  auto bytes = base::as_bytes(base::make_span(s));
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

static const char kSessionStorageDirectory[] = "Session Storage";

class SessionStorageImplTest : public testing::Test {
 public:
  SessionStorageImplTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  ~SessionStorageImplTest() override {
    // There may be pending tasks to clean up files in the temp dir. Make sure
    // they run so temp dir deletion can succeed.
    RunUntilIdle();

    EXPECT_TRUE(temp_dir_.Delete());
  }

  void SetUp() override {
    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &SessionStorageImplTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    if (session_storage_)
      ShutDownSessionStorage();
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  void OnBadMessage(const std::string& reason) { bad_message_called_ = true; }

  void SetBackingMode(SessionStorageImpl::BackingMode backing_mode) {
    DCHECK(!session_storage_);
    backing_mode_ = backing_mode;
  }

  SessionStorageImpl* session_storage() {
    if (!session_storage_) {
      remote_session_storage_.reset();
      session_storage_ = new SessionStorageImpl(
          temp_path(), blocking_task_runner_,
          base::SequencedTaskRunnerHandle::Get(), backing_mode_,
          kSessionStorageDirectory,
          remote_session_storage_.BindNewPipeAndPassReceiver());
    }
    return session_storage_;
  }

  void ShutDownSessionStorage() {
    session_storage_->ShutdownAndDelete();
    session_storage_ = nullptr;
    RunUntilIdle();
  }

  void DoTestPut(const std::string& namespace_id,
                 const url::Origin& origin,
                 base::StringPiece key,
                 base::StringPiece value,
                 const std::string& source) {
    session_storage()->CreateNamespace(namespace_id);
    mojo::Remote<blink::mojom::StorageArea> area;
    session_storage()->BindStorageArea(origin, namespace_id,
                                       area.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector(key),
                              StringPieceToUint8Vector(value), base::nullopt,
                              source));
    session_storage()->DeleteNamespace(namespace_id, true);
  }

  base::Optional<std::vector<uint8_t>> DoTestGet(
      const std::string& namespace_id,
      const url::Origin& origin,
      base::StringPiece key) {
    session_storage()->CreateNamespace(namespace_id);
    mojo::Remote<blink::mojom::StorageArea> area;
    session_storage()->BindStorageArea(origin, namespace_id,
                                       area.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());

    // Use the GetAll interface because Gets are being removed.
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area.get(), &data));
    session_storage()->DeleteNamespace(namespace_id, true);

    std::vector<uint8_t> key_as_bytes = StringPieceToUint8Vector(key);
    for (const auto& key_value : data) {
      if (key_value->key == key_as_bytes) {
        return key_value->value;
      }
    }
    return base::nullopt;
  }

 protected:
  const base::FilePath& temp_path() const { return temp_dir_.GetPath(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  bool bad_message_called_ = false;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  SessionStorageImpl::BackingMode backing_mode_ =
      SessionStorageImpl::BackingMode::kRestoreDiskState;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})};
  SessionStorageImpl* session_storage_ = nullptr;
  mojo::Remote<mojom::SessionStorageControl> remote_session_storage_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageImplTest);
};

TEST_F(SessionStorageImplTest, MigrationV0ToV1) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  base::string16 key = base::ASCIIToUTF16("key");
  base::string16 value = base::ASCIIToUTF16("value");
  base::string16 key2 = base::ASCIIToUTF16("key2");
  key2.push_back(0xd83d);
  key2.push_back(0xde00);

  base::FilePath old_db_path =
      temp_path().AppendASCII(kSessionStorageDirectory);
  {
    auto db = base::MakeRefCounted<TestingLegacySessionStorageDatabase>(
        old_db_path, base::ThreadTaskRunnerHandle::Get().get());
    LegacyDomStorageValuesMap data;
    data[key] = base::NullableString16(value, false);
    data[key2] = base::NullableString16(value, false);
    EXPECT_TRUE(db->CommitAreaChanges(namespace_id1, origin1, false, data));
    EXPECT_TRUE(db->CloneNamespace(namespace_id1, namespace_id2));
  }
  EXPECT_TRUE(base::PathExists(old_db_path));

  // The first call to session_storage() here constructs it.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->CreateNamespace(namespace_id2);

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n2_o1;
  mojo::Remote<blink::mojom::StorageArea> area_n2_o2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2_o1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  session_storage()->BindStorageArea(origin2, namespace_id2,
                                     area_n2_o2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2_o1.get(), &data));
  // There should have been a migration to get rid of the "map-0-" refcount
  // field.
  EXPECT_EQ(2ul, data.size());
  std::vector<uint8_t> key_as_vector =
      StdStringToUint8Vector(base::UTF16ToUTF8(key));
  EXPECT_TRUE(
      base::Contains(data, blink::mojom::KeyValue::New(
                               key_as_vector, String16ToUint8Vector(value))));
  EXPECT_TRUE(
      base::Contains(data, blink::mojom::KeyValue::New(
                               key_as_vector, String16ToUint8Vector(value))));
}

TEST_F(SessionStorageImplTest, StartupShutdownSave) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Verify data is there.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Delete the namespace and shutdown Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and load the persisted namespace.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data from before should be here.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Delete the namespace, shut down Session Storage, and do not persist the
  // data.
  session_storage()->DeleteNamespace(namespace_id1, false);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and the namespace should be empty.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data from before should not be here.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, CloneBeforeBrowserClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();

  // Do the browser-side clone afterwards.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageImplTest, Cloning) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Internally triggered clone before the put. The clone doesn't actually count
  // until a clone comes from the namespace.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();
  area_n1.reset();
  ss_namespace1.reset();

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace so it can be loaded again. This tests the case where our cloning
  // works even though the namespace is deleted (but persisted on disk).
  session_storage()->DeleteNamespace(namespace_id1, true);

  // The data from before should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());

  // Put some data in namespace 2.
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringPieceToUint8Vector("key2"),
                            StringPieceToUint8Vector("value2"), base::nullopt,
                            "source1"));
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(2ul, data.size());

  // Re-open namespace 1, check that we don't have the extra data.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // We should only have the first value.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageImplTest, ImmediateCloning) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  session_storage()->BindNamespace(namespace_id1,
                                   ss_namespace1.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Immediate clone.
  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // Open the second namespace, ensure empty.
  {
    mojo::Remote<blink::mojom::StorageArea> area_n2;
    session_storage()->BindStorageArea(origin1, namespace_id2,
                                       area_n2.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
    EXPECT_EQ(0ul, data.size());
  }

  // Delete that namespace, copy again after a put.
  session_storage()->DeleteNamespace(namespace_id2, false);

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value2"), base::nullopt,
                            "source1"));

  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // Open the second namespace, ensure populated
  {
    mojo::Remote<blink::mojom::StorageArea> area_n2;
    session_storage()->BindStorageArea(origin1, namespace_id2,
                                       area_n2.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
    EXPECT_EQ(1ul, data.size());
  }

  session_storage()->DeleteNamespace(namespace_id2, false);

  // Verify that cloning from the namespace object will result in a bad message.
  session_storage()->CloneNamespace(namespace_id1, namespace_id2,
                                    mojom::SessionStorageCloneType::kImmediate);

  // This should cause a bad message.
  ss_namespace1->Clone(namespace_id2);
  ss_namespace1.FlushForTesting();

  EXPECT_TRUE(bad_message_called_);
}

TEST_F(SessionStorageImplTest, Scavenging) {
  // Create our namespace, shut down Session Storage, and leave that namespace
  // on disk; then verify that it is scavenged if we re-initialize Session
  // Storage without calling CreateNamespace.

  // Create, verify we have no data.
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);

  // This scavenge call should NOT delete the namespace, as we just created it.
  {
    base::RunLoop loop;
    // Cause the connection to start loading, so we start scavenging mid-load.
    session_storage()->Flush(base::DoNothing());
    session_storage()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }
  // Restart Session Storage.
  ShutDownSessionStorage();
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  area_n1.reset();

  // This scavenge call should NOT delete the namespace, as we never called
  // delete.
  session_storage()->ScavengeUnusedNamespaces(base::DoNothing());

  // Restart Session Storage.
  ShutDownSessionStorage();
  session_storage()->CreateNamespace(namespace_id1);

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);

  // This scavenge call should NOT delete the namespace, as we explicitly
  // persisted the namespace.
  {
    base::RunLoop loop;
    session_storage()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }

  ShutDownSessionStorage();

  // Re-initialize Session Storage, load the persisted namespace, and verify we
  // still have data.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();

  // Shutting down Session Storage without an explicit DeleteNamespace
  // should leave the data on disk.
  ShutDownSessionStorage();

  // Re-initialize Session Storage. Scavenge should now remove the namespace as
  // there has been no call to CreateNamespace. Check that the data is
  // empty.
  {
    base::RunLoop loop;
    session_storage()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InvalidVersionOnDisk) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin = url::Origin::Create(GURL("http://foobar.com"));

  // Initialize Session Storage, add some data to it, and check that it's there.
  DoTestPut(namespace_id, origin, "key", "value", "source");
  base::Optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());

  ShutDownSessionStorage();
  {
    // Mess up version number in database.
    leveldb_env::ChromiumEnv env;
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.env = &env;
    base::FilePath db_path =
        temp_path().Append(FILE_PATH_LITERAL("Session Storage"));
    ASSERT_TRUE(leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db).ok());
    ASSERT_TRUE(db->Put(leveldb::WriteOptions(), "version", "argh").ok());
  }

  opt_value = DoTestGet(namespace_id, origin, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, origin, "key", "value", "source");

  ShutDownSessionStorage();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());
  ShutDownSessionStorage();
}

TEST_F(SessionStorageImplTest, CorruptionOnDisk) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin = url::Origin::Create(GURL("http://foobar.com"));

  // Initialize Session Storage, add some data to it, and check that it's there.
  DoTestPut(namespace_id, origin, "key", "value", "source");
  base::Optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());

  ShutDownSessionStorage();
  // Also flush Task Scheduler tasks to make sure the leveldb is fully closed.
  RunUntilIdle();

  // Delete manifest files to mess up opening DB.
  base::FilePath db_path =
      temp_path().Append(FILE_PATH_LITERAL("Session Storage"));
  base::FileEnumerator file_enum(db_path, true, base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("MANIFEST*"));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::DeleteFile(name, false);
  }
  opt_value = DoTestGet(namespace_id, origin, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, origin, "key", "value", "source");

  ShutDownSessionStorage();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());
  ShutDownSessionStorage();
}

TEST_F(SessionStorageImplTest, RecreateOnCommitFailure) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://asf.com"));
  url::Origin origin3 = url::Origin::Create(GURL("http://example.com"));

  base::Optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  session_storage()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));

  open_loop.emplace();

  // Open three connections to the database.
  mojo::Remote<blink::mojom::StorageArea> area_o1;
  mojo::Remote<blink::mojom::StorageArea> area_o2;
  mojo::Remote<blink::mojom::StorageArea> area_o3;
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  session_storage()->CreateNamespace(namespace_id);
  session_storage()->BindNamespace(namespace_id,
                                   ss_namespace.BindNewPipeAndPassReceiver(),
                                   base::DoNothing());
  session_storage()->BindStorageArea(origin1, namespace_id,
                                     area_o1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  session_storage()->BindStorageArea(origin2, namespace_id,
                                     area_o2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  session_storage()->BindStorageArea(origin3, namespace_id,
                                     area_o3.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  open_loop->Run();

  // Ensure that the first opened database always fails to write data.
  session_storage()->GetDatabaseForTesting().PostTaskWithThisObject(
      FROM_HERE, base::BindLambdaForTesting([&](DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until SessionStorageImpl tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();

  // Also prepare for another database connection, next time providing a
  // functioning database.
  open_loop.emplace();
  session_storage()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first origin. This put operation should result in a
  // pending commit that will get cancelled when the database connection is
  // closed.
  auto value = StringPieceToUint8Vector("avalue");
  area_o3->Put(StringPieceToUint8Vector("w3key"), value, base::nullopt,
               "source",
               base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  int i = 0;
  while (area_o1.is_connected()) {
    ++i;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    std::vector<uint8_t> old_value = value;
    value[0]++;
    area_o1->Put(StringPieceToUint8Vector("key"), value, base::nullopt,
                 "source", base::BindLambdaForTesting([&](bool success) {
                   EXPECT_TRUE(success);
                 }));
    area_o1.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage()->FlushAreaForTesting(namespace_id, origin1);
  }
  area_o1.reset();

  // Wait for a new database to be opened, which should happen after the first
  // database is destroyed due to failures.
  open_loop->Run();
  EXPECT_EQ(1u, num_databases_destroyed);
  EXPECT_EQ(2u, num_database_open_requests);

  // The connection to the second area should have closed as well.
  area_o2.FlushForTesting();
  ss_namespace.FlushForTesting();
  EXPECT_FALSE(area_o2.is_connected());
  EXPECT_FALSE(ss_namespace.is_connected());

  // Reconnect area_o1 to the new database, and try to read a value.
  ss_namespace.reset();
  session_storage()->BindStorageArea(origin1, namespace_id,
                                     area_o1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  base::RunLoop delete_loop;
  bool success = true;
  test::MockLevelDBObserver observer4;
  area_o1->AddObserver(observer4.Bind());
  area_o1->Delete(StringPieceToUint8Vector("key"), base::nullopt, "source",
                  base::BindLambdaForTesting([&](bool success_in) {
                    success = success_in;
                    delete_loop.Quit();
                  }));

  // And deleting the value from the new area should have failed (as the
  // database is empty).
  delete_loop.Run();
  area_o1.reset();
  session_storage()->DeleteNamespace(namespace_id, true);

  {
    // Committing data should now work.
    DoTestPut(namespace_id, origin1, "key", "value", "source");
    base::Optional<std::vector<uint8_t>> opt_value =
        DoTestGet(namespace_id, origin1, "key");
    ASSERT_TRUE(opt_value);
    EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());
  }
}

TEST_F(SessionStorageImplTest, DontRecreateOnRepeatedCommitFailure) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  base::Optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  session_storage()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();
      }));
  open_loop.emplace();

  // Open three connections to the database.
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->CreateNamespace(namespace_id);
  session_storage()->BindStorageArea(origin1, namespace_id,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  open_loop->Run();

  // Ensure that this database always fails to write data.
  session_storage()->GetDatabaseForTesting().PostTaskWithThisObject(
      FROM_HERE, base::BindLambdaForTesting([&](DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  EXPECT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until SessionStorageImpl tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();
  session_storage()->SetDatabaseOpenCallbackForTesting(
      base::BindLambdaForTesting([&] {
        ++num_database_open_requests;
        open_loop->Quit();

        // Ensure that this database also always fails to write data.
        session_storage()->GetDatabaseForTesting().Post(
            FROM_HERE, &DomStorageDatabase::MakeAllCommitsFailForTesting);
      }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  auto value = StringPieceToUint8Vector("avalue");
  base::Optional<std::vector<uint8_t>> old_value = base::nullopt;
  while (area.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringPieceToUint8Vector("key"), value, old_value, "source",
              base::BindLambdaForTesting(
                  [&](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage()->FlushAreaForTesting(namespace_id, origin1);

    old_value = value;
    value[0]++;
  }
  area.reset();

  // Wait for SessionStorageImpl to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  open_loop->Run();

  EXPECT_EQ(2u, num_database_open_requests);
  EXPECT_EQ(1u, num_databases_destroyed);

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  session_storage()->BindStorageArea(origin1, namespace_id,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  old_value = base::nullopt;
  for (int i = 0; i < 64; ++i) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringPieceToUint8Vector("key"), value, old_value, "source",
              base::BindLambdaForTesting(
                  [&](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    session_storage()->FlushAreaForTesting(namespace_id, origin1);

    old_value = value;
    value[0]++;
  }

  // Should still be connected after all that.
  RunUntilIdle();
  EXPECT_TRUE(area.is_connected());

  session_storage()->DeleteNamespace(namespace_id, false);
  ShutDownSessionStorage();
}

TEST_F(SessionStorageImplTest, GetUsage) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  base::RunLoop loop;
  session_storage()->GetUsage(base::BindLambdaForTesting(
      [&](std::vector<mojom::SessionStorageUsageInfoPtr> usage) {
        loop.Quit();
        ASSERT_EQ(1u, usage.size());
        EXPECT_EQ(origin1, usage[0]->origin);
        EXPECT_EQ(namespace_id1, usage[0]->namespace_id);
      }));
  loop.Run();
}

TEST_F(SessionStorageImplTest, DeleteStorage) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  // First, test deleting data for a namespace that is open.
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  session_storage()->DeleteStorage(origin1, namespace_id1, base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Next, test that it deletes the data even if there isn't a namespace open.
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  area.reset();

  // Delete the namespace and shutdown Session Storage, BUT persist the
  // namespace so it can be loaded again.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This re-initializes Session Storage, then deletes the storage.
  session_storage()->DeleteStorage(origin1, namespace_id1, base::DoNothing());

  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  data.clear();
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, PurgeInactiveWrappers) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  session_storage()->FlushAreaForTesting(namespace_id1, origin1);

  area.reset();

  // Clear all the data from the backing database.
  base::RunLoop loop;
  session_storage()->DatabaseForTesting()->DeletePrefixed(
      StringPieceToUint8Vector("map"),
      base::BindLambdaForTesting([&](leveldb::Status status) {
        loop.Quit();
        EXPECT_TRUE(status.ok());
      }));
  loop.Run();

  // Now open many new wrappers (for different origins) to trigger clean up.
  for (int i = 1; i <= 100; ++i) {
    const url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("http://example.com:%d", i)));
    session_storage()->BindStorageArea(origin, namespace_id1,
                                       area.BindNewPipeAndPassReceiver(),
                                       base::DoNothing());
    RunUntilIdle();
    area.reset();
  }

  // And make sure caches were actually cleared.
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());
  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

// TODO(https://crbug.com/1008697): Flakes when verifying no data found.
TEST_F(SessionStorageImplTest, ClearDiskState) {
  SetBackingMode(SessionStorageImpl::BackingMode::kClearDiskStateOnOpen);
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);

  mojo::Remote<blink::mojom::StorageArea> area;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  area.reset();

  // Delete the namespace and shut down Session Storage, BUT persist the
  // namespace on disk.
  session_storage()->DeleteNamespace(namespace_id1, true);
  ShutDownSessionStorage();

  // This will re-initialize Session Storage and load the persisted namespace,
  // but it should have been deleted due to our backing mode.
  session_storage()->CreateNamespace(namespace_id1);
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data from before should not be here, because SessionStorageImpl
  // clears disk space on open.
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InterruptedCloneWithDelete) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id1, false);

  // Open the second namespace which should be initialized and empty.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InterruptedCloneChainWithDelete) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id2, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n3;
  session_storage()->BindStorageArea(origin1, namespace_id3,
                                     area_n3.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n3.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, InterruptedTripleCloneChain) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  std::string namespace_id4 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id3, namespace_id4,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id3, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n4;
  session_storage()->BindStorageArea(origin1, namespace_id4,
                                     area_n4.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Trigger the populated of namespace 2 by deleting namespace 1.
  session_storage()->DeleteNamespace(namespace_id1, false);

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n4.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, TotalCloneChainDeletion) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  std::string namespace_id4 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);

  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id2, namespace_id3,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->CloneNamespace(
      namespace_id3, namespace_id4,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  session_storage()->DeleteNamespace(namespace_id2, false);
  session_storage()->DeleteNamespace(namespace_id3, false);
  session_storage()->DeleteNamespace(namespace_id1, false);
  session_storage()->DeleteNamespace(namespace_id4, false);
}

}  // namespace

TEST_F(SessionStorageImplTest, PurgeMemoryDoesNotCrashOrHang) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  session_storage()->CreateNamespace(namespace_id2);
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value2"), base::nullopt,
                            "source1"));

  session_storage()->FlushAreaForTesting(namespace_id1, origin1);

  area_n2.reset();

  RunUntilIdle();

  // Verify this doesn't crash or hang.
  session_storage()->PurgeMemory();

  size_t memory_used = session_storage()
                           ->namespaces_[namespace_id1]
                           ->origin_areas_[origin1]
                           ->data_map()
                           ->storage_area()
                           ->memory_used();
  EXPECT_EQ(0ul, memory_used);

  // Test the values is still there.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  base::Optional<std::vector<uint8_t>> opt_value2 =
      DoTestGet(namespace_id2, origin1, "key1");
  ASSERT_TRUE(opt_value2);
  EXPECT_EQ(StringPieceToUint8Vector("value2"), opt_value2.value());
}

TEST_F(SessionStorageImplTest, DeleteWithPersistBeforeBrowserClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Delete the origin namespace, but save it.
  session_storage()->DeleteNamespace(namespace_id1, true);

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageImplTest, DeleteWithoutPersistBeforeBrowserClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Delete the origin namespace and don't save it.
  session_storage()->DeleteNamespace(namespace_id1, false);

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be gone, because the first namespace wasn't saved to disk.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageImplTest, DeleteAfterCloneWithoutMojoClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  session_storage()->CreateNamespace(namespace_id1);
  mojo::Remote<blink::mojom::StorageArea> area_n1;
  session_storage()->BindStorageArea(origin1, namespace_id1,
                                     area_n1.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Do the browser-side clone.
  session_storage()->CloneNamespace(
      namespace_id1, namespace_id2,
      mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);

  // Delete the origin namespace and don't save it.
  session_storage()->DeleteNamespace(namespace_id1, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::StorageArea> area_n2;
  session_storage()->BindStorageArea(origin1, namespace_id2,
                                     area_n2.BindNewPipeAndPassReceiver(),
                                     base::DoNothing());

  // The data should be there, as the namespace should clone to all pending
  // namespaces on destruction if it didn't get a 'Clone' from mojo.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

}  // namespace storage
