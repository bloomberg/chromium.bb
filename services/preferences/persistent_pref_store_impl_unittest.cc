// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/persistent_pref_store_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/prefs/in_memory_pref_store.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/cpp/persistent_pref_store_client.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::WithoutArgs;

namespace prefs {
namespace {

class PrefStoreObserverMock : public PrefStore::Observer {
 public:
  MOCK_METHOD1(OnPrefValueChanged, void(const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void(bool succeeded));
};

class PersistentPrefStoreMock : public InMemoryPrefStore {
 public:
  MOCK_METHOD0(CommitPendingWrite, void());
  MOCK_METHOD0(SchedulePendingLossyWrites, void());
  MOCK_METHOD0(ClearMutableValues, void());

 private:
  ~PersistentPrefStoreMock() override = default;
};

class PrefStoreConnectorMock : public mojom::PrefStoreConnector {
 public:
  MOCK_METHOD1(Connect, void(const ConnectCallback&));
};

class InitializationMockPersistentPrefStore : public InMemoryPrefStore {
 public:
  bool IsInitializationComplete() const override { return initialized_; }

  void AddObserver(PrefStore::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(PrefStore::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override {
    DCHECK(!error_delegate);
  }

  PersistentPrefStore::PrefReadError GetReadError() const override {
    return read_error_;
  }
  bool ReadOnly() const override { return read_only_; }

  void Initialize(bool success,
                  PersistentPrefStore::PrefReadError error,
                  bool read_only) {
    initialized_ = success;
    read_error_ = error;
    read_only_ = read_only;
    for (auto& observer : observers_) {
      observer.OnInitializationCompleted(initialized_);
    }
  }

 private:
  ~InitializationMockPersistentPrefStore() override = default;

  PersistentPrefStore::PrefReadError read_error_;
  bool read_only_ = false;
  bool initialized_ = false;
  base::ObserverList<PrefStore::Observer, true> observers_;
};

class PersistentPrefStoreImplTest : public testing::Test {
 public:
  PersistentPrefStoreImplTest() = default;

  // testing::Test:
  void TearDown() override {
    pref_store_ = nullptr;
    base::RunLoop().RunUntilIdle();
    bindings_.CloseAllBindings();
    backing_pref_store_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateImpl(scoped_refptr<PersistentPrefStore> backing_pref_store) {
    backing_pref_store_ = base::MakeUnique<PersistentPrefStoreImpl>(
        std::move(backing_pref_store), nullptr);
    mojo::Binding<mojom::PersistentPrefStoreConnector> binding(
        backing_pref_store_.get());
    pref_store_ = CreateConnection();
  }

  mojom::PersistentPrefStoreConnectorPtr CreateConnector() {
    return bindings_.CreateInterfacePtrAndBind(backing_pref_store_.get());
  }

  scoped_refptr<PersistentPrefStore> CreateConnection() {
    return make_scoped_refptr(new PersistentPrefStoreClient(
        bindings_.CreateInterfacePtrAndBind(backing_pref_store_.get())));
  }

  PersistentPrefStore* pref_store() { return pref_store_.get(); }

 private:
  base::MessageLoop message_loop_;

  std::unique_ptr<PersistentPrefStoreImpl> backing_pref_store_;
  mojo::BindingSet<mojom::PersistentPrefStoreConnector> bindings_;

  scoped_refptr<PersistentPrefStore> pref_store_;

  DISALLOW_COPY_AND_ASSIGN(PersistentPrefStoreImplTest);
};

TEST_F(PersistentPrefStoreImplTest, InitializationSuccess) {
  auto backing_pref_store =
      make_scoped_refptr(new InitializationMockPersistentPrefStore);
  CreateImpl(backing_pref_store);
  backing_pref_store->Initialize(
      true, PersistentPrefStore::PREF_READ_ERROR_NONE, false);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->GetReadError());
  EXPECT_FALSE(pref_store()->ReadOnly());
}

TEST_F(PersistentPrefStoreImplTest, InitializationFailure) {
  auto backing_pref_store =
      make_scoped_refptr(new InitializationMockPersistentPrefStore);
  CreateImpl(backing_pref_store);
  backing_pref_store->Initialize(
      false, PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE, true);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE,
            pref_store()->ReadPrefs());
  EXPECT_FALSE(pref_store()->IsInitializationComplete());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE,
            pref_store()->GetReadError());
  EXPECT_TRUE(pref_store()->ReadOnly());
}

class TestReadErrorDelegate : public PersistentPrefStore::ReadErrorDelegate {
 public:
  TestReadErrorDelegate(PersistentPrefStore::PrefReadError* storage,
                        const base::Closure& quit)
      : storage_(storage), quit_(quit) {
    DCHECK(storage_);
    DCHECK(quit_);
  }

  void OnError(PersistentPrefStore::PrefReadError error) override {
    *storage_ = error;
    quit_.Run();
  }

 private:
  PersistentPrefStore::PrefReadError* const storage_;
  const base::Closure quit_;
};

TEST_F(PersistentPrefStoreImplTest, InitializationFailure_AsyncRead) {
  auto backing_pref_store =
      make_scoped_refptr(new InitializationMockPersistentPrefStore);
  CreateImpl(backing_pref_store);
  backing_pref_store->Initialize(
      false, PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE, true);
  PersistentPrefStore::PrefReadError read_error =
      PersistentPrefStore::PREF_READ_ERROR_NONE;
  base::RunLoop run_loop;
  pref_store()->ReadPrefsAsync(
      new TestReadErrorDelegate(&read_error, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(pref_store()->IsInitializationComplete());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE, read_error);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE,
            pref_store()->GetReadError());
  EXPECT_TRUE(pref_store()->ReadOnly());
}

TEST_F(PersistentPrefStoreImplTest, DelayedInitializationSuccess) {
  auto backing_pref_store =
      make_scoped_refptr(new InitializationMockPersistentPrefStore);

  CreateImpl(backing_pref_store);
  auto connector = CreateConnector();
  base::RunLoop run_loop;
  connector->Connect(base::Bind(
      [](const base::Closure& quit,
         PersistentPrefStore::PrefReadError read_error, bool read_only,
         std::unique_ptr<base::DictionaryValue> local_prefs,
         mojom::PersistentPrefStorePtr pref_store,
         mojom::PrefStoreObserverRequest observer_request) {
        quit.Run();
        EXPECT_FALSE(read_only);
        EXPECT_TRUE(local_prefs);
        EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, read_error);
      },
      run_loop.QuitClosure()));
  connector.FlushForTesting();
  backing_pref_store->Initialize(
      true, PersistentPrefStore::PREF_READ_ERROR_NONE, false);
  run_loop.Run();
}

TEST_F(PersistentPrefStoreImplTest, DelayedInitializationFailure) {
  auto backing_pref_store =
      make_scoped_refptr(new InitializationMockPersistentPrefStore);

  CreateImpl(backing_pref_store);
  auto connector = CreateConnector();
  base::RunLoop run_loop;
  connector->Connect(base::Bind(
      [](const base::Closure& quit,
         PersistentPrefStore::PrefReadError read_error, bool read_only,
         std::unique_ptr<base::DictionaryValue> local_prefs,
         mojom::PersistentPrefStorePtr pref_store,
         mojom::PrefStoreObserverRequest observer_request) {
        quit.Run();
        EXPECT_TRUE(read_only);
        EXPECT_FALSE(local_prefs);
        EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED,
                  read_error);
      },
      run_loop.QuitClosure()));
  connector.FlushForTesting();
  backing_pref_store->Initialize(
      false, PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED, true);
  run_loop.Run();
}

constexpr char kKey[] = "path.to.key";

TEST_F(PersistentPrefStoreImplTest, InitialValue) {
  auto backing_pref_store = make_scoped_refptr(new InMemoryPrefStore());
  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PersistentPrefStoreImplTest, InitialValueWithoutPathExpansion) {
  auto backing_pref_store = make_scoped_refptr(new InMemoryPrefStore());
  base::DictionaryValue dict;
  dict.SetStringWithoutPathExpansion(kKey, "value");
  backing_pref_store->SetValue(kKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PersistentPrefStoreImplTest, WriteObservedByOtherClient) {
  auto backing_pref_store = make_scoped_refptr(new InMemoryPrefStore());
  CreateImpl(backing_pref_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            other_pref_store->ReadPrefs());
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  const base::Value value("value");
  pref_store()->SetValueSilently(kKey, value.CreateDeepCopy(), 0);

  PrefStoreObserverMock observer;
  other_pref_store->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPrefValueChanged(kKey))
      .Times(1)
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  run_loop.Run();
  other_pref_store->RemoveObserver(&observer);

  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PersistentPrefStoreImplTest,
       WriteWithoutPathExpansionObservedByOtherClient) {
  auto backing_pref_store = make_scoped_refptr(new InMemoryPrefStore());
  CreateImpl(backing_pref_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            other_pref_store->ReadPrefs());
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  base::DictionaryValue dict;
  dict.SetStringWithoutPathExpansion(kKey, "value");
  pref_store()->SetValue(kKey, dict.CreateDeepCopy(), 0);

  PrefStoreObserverMock observer;
  other_pref_store->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPrefValueChanged(kKey))
      .Times(1)
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  run_loop.Run();
  other_pref_store->RemoveObserver(&observer);

  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PersistentPrefStoreImplTest, RemoveObservedByOtherClient) {
  auto backing_pref_store = make_scoped_refptr(new InMemoryPrefStore());
  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            other_pref_store->ReadPrefs());
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
  pref_store()->RemoveValue(kKey, 0);

  // This should be a no-op and shouldn't trigger a notification for the other
  // client.
  pref_store()->RemoveValue(kKey, 0);

  PrefStoreObserverMock observer;
  other_pref_store->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPrefValueChanged(kKey))
      .Times(1)
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  run_loop.Run();
  base::RunLoop().RunUntilIdle();
  other_pref_store->RemoveObserver(&observer);

  EXPECT_FALSE(other_pref_store->GetValue(kKey, &output));
}

TEST_F(PersistentPrefStoreImplTest,
       RemoveWithoutPathExpansionObservedByOtherClient) {
  auto backing_pref_store = make_scoped_refptr(new InMemoryPrefStore());
  base::DictionaryValue dict;
  dict.SetStringWithoutPathExpansion(kKey, "value");
  backing_pref_store->SetValue(kKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            other_pref_store->ReadPrefs());
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));

  base::Value* mutable_value = nullptr;
  dict.SetStringWithoutPathExpansion(kKey, "value");
  ASSERT_TRUE(pref_store()->GetMutableValue(kKey, &mutable_value));
  base::DictionaryValue* mutable_dict = nullptr;
  ASSERT_TRUE(mutable_value->GetAsDictionary(&mutable_dict));
  mutable_dict->RemoveWithoutPathExpansion(kKey, nullptr);
  pref_store()->ReportValueChanged(kKey, 0);

  PrefStoreObserverMock observer;
  other_pref_store->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPrefValueChanged(kKey))
      .Times(1)
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  run_loop.Run();
  other_pref_store->RemoveObserver(&observer);

  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  const base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(output->GetAsDictionary(&dict_value));
  EXPECT_TRUE(dict_value->empty());
}

TEST_F(PersistentPrefStoreImplTest, CommitPendingWrite) {
  auto backing_store = make_scoped_refptr(new PersistentPrefStoreMock);
  CreateImpl(backing_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  base::RunLoop run_loop;
  EXPECT_CALL(*backing_store, CommitPendingWrite())
      .Times(2)
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  pref_store()->CommitPendingWrite();
  run_loop.Run();
}

TEST_F(PersistentPrefStoreImplTest, SchedulePendingLossyWrites) {
  auto backing_store = make_scoped_refptr(new PersistentPrefStoreMock);
  CreateImpl(backing_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  base::RunLoop run_loop;
  EXPECT_CALL(*backing_store, SchedulePendingLossyWrites())
      .Times(1)
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  EXPECT_CALL(*backing_store, CommitPendingWrite()).Times(1);
  pref_store()->SchedulePendingLossyWrites();
  run_loop.Run();
}

TEST_F(PersistentPrefStoreImplTest, ClearMutableValues) {
  auto backing_store = make_scoped_refptr(new PersistentPrefStoreMock);
  CreateImpl(backing_store);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            pref_store()->ReadPrefs());
  base::RunLoop run_loop;
  EXPECT_CALL(*backing_store, ClearMutableValues())
      .Times(1)
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  EXPECT_CALL(*backing_store, CommitPendingWrite()).Times(1);
  pref_store()->ClearMutableValues();
  run_loop.Run();
}

}  // namespace
}  // namespace prefs
