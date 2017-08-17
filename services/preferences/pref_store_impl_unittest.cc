// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/pref_store_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/prefs/value_map_pref_store.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/cpp/pref_store_client.h"
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
  MOCK_METHOD1(OnInitializationCompleted, void(bool));
};

class MockPrefStore : public ValueMapPrefStore {
 public:
  bool IsInitializationComplete() const override {
    return initialized_ && success_;
  }

  void AddObserver(PrefStore::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(PrefStore::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void CompleteInitialization(bool success) {
    initialized_ = true;
    success_ = success;
    for (auto& observer : observers_) {
      // Some pref stores report completing initialization more than once. Test
      // that additional calls are ignored.
      observer.OnInitializationCompleted(success);
      observer.OnInitializationCompleted(success);
    }
  }

  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override {
    ValueMapPrefStore::SetValue(key, std::move(value), flags);
    for (auto& observer : observers_) {
      observer.OnPrefValueChanged(key);
    }
  }

 private:
  ~MockPrefStore() override = default;

  bool initialized_ = false;
  bool success_ = false;
  base::ObserverList<PrefStore::Observer, true> observers_;
};

constexpr char kKey[] = "path.to.key";
constexpr char kOtherKey[] = "path.to.other_key";

void ExpectInitializationComplete(PrefStore* pref_store, bool success) {
  PrefStoreObserverMock observer;
  pref_store->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPrefValueChanged("")).Times(0);
  EXPECT_CALL(observer, OnInitializationCompleted(success))
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  run_loop.Run();
  pref_store->RemoveObserver(&observer);
}

void ExpectPrefChange(PrefStore* pref_store, base::StringPiece key) {
  PrefStoreObserverMock observer;
  pref_store->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPrefValueChanged(key.as_string()))
      .WillOnce(WithoutArgs(Invoke([&run_loop]() { run_loop.Quit(); })));
  run_loop.Run();
  pref_store->RemoveObserver(&observer);
}

class PrefStoreImplTest : public testing::Test {
 public:
  PrefStoreImplTest() = default;

  // testing::Test:
  void TearDown() override {
    pref_store_ = nullptr;
    base::RunLoop().RunUntilIdle();
    impl_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateImpl(
      scoped_refptr<PrefStore> backing_pref_store,
      std::vector<std::string> observed_prefs = std::vector<std::string>()) {
    impl_ = base::MakeUnique<PrefStoreImpl>(std::move(backing_pref_store));

    if (observed_prefs.empty())
      observed_prefs.insert(observed_prefs.end(), {kKey, kOtherKey});
    pref_store_ = make_scoped_refptr(
        new PrefStoreClient(impl_->AddObserver(observed_prefs)));
  }

  PrefStore* pref_store() { return pref_store_.get(); }

 private:
  base::MessageLoop message_loop_;

  std::unique_ptr<PrefStoreImpl> impl_;

  scoped_refptr<PrefStore> pref_store_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreImplTest);
};

TEST_F(PrefStoreImplTest, InitializationSuccess) {
  auto backing_pref_store = make_scoped_refptr(new MockPrefStore());
  backing_pref_store->SetValue(kKey, base::MakeUnique<base::Value>("value"), 0);
  CreateImpl(backing_pref_store);
  EXPECT_FALSE(pref_store()->IsInitializationComplete());

  backing_pref_store->CompleteInitialization(true);
  ExpectInitializationComplete(pref_store(), true);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
}

TEST_F(PrefStoreImplTest, InitializationFailure) {
  auto backing_pref_store = make_scoped_refptr(new MockPrefStore());
  backing_pref_store->SetValue(kKey, base::MakeUnique<base::Value>("value"), 0);
  CreateImpl(backing_pref_store);
  EXPECT_FALSE(pref_store()->IsInitializationComplete());

  backing_pref_store->CompleteInitialization(false);
  ExpectInitializationComplete(pref_store(), false);

  // TODO(sammc): Should IsInitializationComplete() return false?
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
}

TEST_F(PrefStoreImplTest, ValueChangesBeforeInitializationCompletes) {
  auto backing_pref_store = make_scoped_refptr(new MockPrefStore());
  CreateImpl(backing_pref_store);
  EXPECT_FALSE(pref_store()->IsInitializationComplete());

  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  backing_pref_store->CompleteInitialization(true);

  // The update occurs before initialization has completed, so should not
  // trigger notifications to client observers, but the value should be
  // observable once initialization completes.
  ExpectInitializationComplete(pref_store(), true);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PrefStoreImplTest, InitialValue) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PrefStoreImplTest, InitialValueWithoutPathExpansion) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  base::DictionaryValue dict;
  dict.SetKey(kKey, base::Value("value"));
  backing_pref_store->SetValue(kKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PrefStoreImplTest, WriteObservedByClient) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);

  ExpectPrefChange(pref_store(), kKey);
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PrefStoreImplTest, WriteToUnregisteredPrefNotObservedByClient) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  CreateImpl(backing_pref_store, {kKey});
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  backing_pref_store->SetValue(kOtherKey, base::MakeUnique<base::Value>(123),
                               0);
  backing_pref_store->SetValue(kKey, base::MakeUnique<base::Value>("value"), 0);

  ExpectPrefChange(pref_store(), kKey);
  EXPECT_FALSE(pref_store()->GetValue(kOtherKey, nullptr));
}

TEST_F(PrefStoreImplTest, WriteWithoutPathExpansionObservedByClient) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  base::DictionaryValue dict;
  dict.SetKey(kKey, base::Value("value"));
  backing_pref_store->SetValue(kKey, dict.CreateDeepCopy(), 0);

  ExpectPrefChange(pref_store(), kKey);
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PrefStoreImplTest, RemoveObservedByClient) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
  backing_pref_store->RemoveValue(kKey, 0);

  // This should be a no-op and shouldn't trigger a notification for the other
  // client.
  backing_pref_store->RemoveValue(kKey, 0);

  ExpectPrefChange(pref_store(), kKey);
  EXPECT_FALSE(pref_store()->GetValue(kKey, &output));
}

TEST_F(PrefStoreImplTest, RemoveOfUnregisteredPrefNotObservedByClient) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  backing_pref_store->SetValue(kOtherKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store, {kKey});
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  backing_pref_store->RemoveValue(kOtherKey, 0);
  backing_pref_store->RemoveValue(kKey, 0);

  ExpectPrefChange(pref_store(), kKey);
}

TEST_F(PrefStoreImplTest, RemoveWithoutPathExpansionObservedByOtherClient) {
  auto backing_pref_store = make_scoped_refptr(new ValueMapPrefStore());
  base::DictionaryValue dict;
  dict.SetKey(kKey, base::Value("value"));
  backing_pref_store->SetValue(kKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));

  base::Value* mutable_value = nullptr;
  dict.SetKey(kKey, base::Value("value"));
  ASSERT_TRUE(backing_pref_store->GetMutableValue(kKey, &mutable_value));
  base::DictionaryValue* mutable_dict = nullptr;
  ASSERT_TRUE(mutable_value->GetAsDictionary(&mutable_dict));
  mutable_dict->RemoveWithoutPathExpansion(kKey, nullptr);
  backing_pref_store->ReportValueChanged(kKey, 0);

  ExpectPrefChange(pref_store(), kKey);
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  const base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(output->GetAsDictionary(&dict_value));
  EXPECT_TRUE(dict_value->empty());
}

}  // namespace
}  // namespace prefs
