// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_client_store.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/prefs/pref_store_observer_mock.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test implmentation of prefs::mojom::PreferenceManager which simply tracks
// calls. Allows for testing to be done in process, without mojo IPC.
class TestPreferenceService : public prefs::mojom::PreferencesService {
 public:
  TestPreferenceService(
      mojo::InterfaceRequest<prefs::mojom::PreferencesService> request)
      : set_preferences_called_(false), binding_(this, std::move(request)) {}
  ~TestPreferenceService() override {}

  const std::set<std::string>& last_preference_set() {
    return last_preference_set_;
  }
  bool set_preferences_called() { return set_preferences_called_; }

  // prefs::mojom::TestPreferenceService:
  void SetPreferences(
      std::unique_ptr<base::DictionaryValue> preferences) override;
  void Subscribe(const std::vector<std::string>& preferences) override;

 private:
  std::set<std::string> last_preference_set_;
  bool set_preferences_called_;
  mojo::Binding<prefs::mojom::PreferencesService> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestPreferenceService);
};

void TestPreferenceService::SetPreferences(
    std::unique_ptr<base::DictionaryValue> preferences) {
  set_preferences_called_ = true;
}

void TestPreferenceService::Subscribe(
    const std::vector<std::string>& preferences) {
  last_preference_set_.clear();
  last_preference_set_.insert(preferences.begin(), preferences.end());
}

// Test implementation of prefs::mojom::PreferencesServiceFactory which simply
// creates the TestPreferenceService used for testing.
class TestPreferenceFactory : public prefs::mojom::PreferencesServiceFactory {
 public:
  TestPreferenceFactory(
      mojo::InterfaceRequest<prefs::mojom::PreferencesServiceFactory> request)
      : binding_(this, std::move(request)) {}
  ~TestPreferenceFactory() override {}

  TestPreferenceService* service() { return service_.get(); }

  void Create(prefs::mojom::PreferencesServiceClientPtr observer,
              prefs::mojom::PreferencesServiceRequest service) override;

 private:
  mojo::Binding<prefs::mojom::PreferencesServiceFactory> binding_;
  std::unique_ptr<TestPreferenceService> service_;

  DISALLOW_COPY_AND_ASSIGN(TestPreferenceFactory);
};

void TestPreferenceFactory::Create(
    prefs::mojom::PreferencesServiceClientPtr observer,
    prefs::mojom::PreferencesServiceRequest service) {
  service_.reset(new TestPreferenceService(std::move(service)));
}

}  // namespace

namespace preferences {

class PrefClientStoreTest : public testing::Test {
 public:
  PrefClientStoreTest() {}
  ~PrefClientStoreTest() override {}

  TestPreferenceService* service() { return factory_->service(); }
  PrefStoreObserverMock* observer() { return &observer_; }
  PrefClientStore* store() { return store_.get(); }

  bool Initialized() { return store_->initialized_; }
  void OnPreferencesChanged(const base::DictionaryValue& preferences) {
    store_->OnPreferencesChanged(preferences.CreateDeepCopy());
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  scoped_refptr<PrefClientStore> store_;
  prefs::mojom::PreferencesServiceFactoryPtr factory_proxy_;
  std::unique_ptr<TestPreferenceFactory> factory_;
  PrefStoreObserverMock observer_;
  // Required by mojo binding code within PrefClientStore.
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PrefClientStoreTest);
};

void PrefClientStoreTest::SetUp() {
  factory_.reset(new TestPreferenceFactory(mojo::MakeRequest(&factory_proxy_)));
  store_ = new PrefClientStore(std::move(factory_proxy_));
  base::RunLoop().RunUntilIdle();
  store_->AddObserver(&observer_);
}

void PrefClientStoreTest::TearDown() {
  store_->RemoveObserver(&observer_);
}

// Tests that observers are notified upon the completion of initialization, and
// that values become available.
TEST_F(PrefClientStoreTest, Initialization) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Subscribe(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const int kValue = 42;
  base::Value pref(kValue);
  base::DictionaryValue prefs;
  prefs.Set(key, pref.CreateDeepCopy());

  // PreferenceManager notifies of PreferencesChanged, completing
  // initialization.
  OnPreferencesChanged(prefs);
  EXPECT_TRUE(Initialized());
  EXPECT_TRUE(observer()->initialized);
  EXPECT_TRUE(observer()->initialization_success);
  observer()->VerifyAndResetChangedKey(key);

  const base::Value* value = nullptr;
  int actual_value;
  EXPECT_TRUE(store()->GetValue(key, &value));
  EXPECT_NE(nullptr, value);
  EXPECT_TRUE(value->GetAsInteger(&actual_value));
  EXPECT_EQ(kValue, actual_value);
  EXPECT_FALSE(service()->set_preferences_called());
}

// Tests that values set silently are also set on the preference service, but
// that no observers are notified.
TEST_F(PrefClientStoreTest, SetValueSilently) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Subscribe(keys);

  const int kValue = 42;
  base::Value pref(kValue);
  store()->SetValueSilently(key, pref.CreateDeepCopy(), 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service()->set_preferences_called());
  EXPECT_TRUE(observer()->changed_keys.empty());
}

// Test that reporting values changed notifies observers, and the preference
// service.
TEST_F(PrefClientStoreTest, ReportValueChanged) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Subscribe(keys);

  const int kValue = 42;
  base::Value pref(kValue);
  base::DictionaryValue prefs;
  prefs.Set(key, pref.CreateDeepCopy());
  OnPreferencesChanged(prefs);
  observer()->changed_keys.clear();

  store()->ReportValueChanged(key, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service()->set_preferences_called());
  observer()->VerifyAndResetChangedKey(key);
}

// Test that when initialized with multiple keys, that observers receive a
// notification for each key.
TEST_F(PrefClientStoreTest, MultipleKeyInitialization) {
  std::set<std::string> keys;
  const std::string key1("hey");
  const std::string key2("listen");
  keys.insert(key1);
  keys.insert(key2);
  store()->Subscribe(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const int kValue = 42;
  base::Value pref1(kValue);
  const std::string kStringValue("look");
  base::StringValue pref2(kStringValue);

  base::DictionaryValue prefs;
  prefs.Set(key1, pref1.CreateDeepCopy());
  prefs.Set(key2, pref2.CreateDeepCopy());

  // The observer should be notified of all keys set.
  OnPreferencesChanged(prefs);
  EXPECT_TRUE(Initialized());
  EXPECT_TRUE(observer()->initialized);
  EXPECT_EQ(2u, observer()->changed_keys.size());
  EXPECT_NE(observer()->changed_keys.end(),
            std::find(observer()->changed_keys.begin(),
                      observer()->changed_keys.end(), key1));
  EXPECT_NE(observer()->changed_keys.end(),
            std::find(observer()->changed_keys.begin(),
                      observer()->changed_keys.end(), key2));
}

// Tests that if OnPreferencesChanged is received with invalid keys, that they
// are ignored.
TEST_F(PrefClientStoreTest, InvalidInitialization) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Subscribe(keys);

  const std::string kInvalidKey("look");
  const int kValue = 42;
  base::Value pref(kValue);
  base::DictionaryValue prefs;
  prefs.Set(kInvalidKey, pref.CreateDeepCopy());

  OnPreferencesChanged(prefs);
  EXPECT_TRUE(observer()->changed_keys.empty());
}

// Tests that when tracking preferences which nest other DictionaryValues, that
// modifications to the nested values properly notify the observer.
TEST_F(PrefClientStoreTest, WriteToNestedPrefs) {
  std::set<std::string> keys;
  const std::string key1("hey");
  const std::string key2("listen");
  keys.insert(key1);
  keys.insert(key2);
  store()->Subscribe(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const std::string sub_key1("look");
  const int kValue1 = 42;
  base::Value pref1(kValue1);
  base::DictionaryValue sub_dictionary1;
  sub_dictionary1.Set(sub_key1, pref1.CreateDeepCopy());

  const std::string sub_key2("Watch out!\n");
  const int kValue2 = 1337;
  base::Value pref2(kValue2);
  base::DictionaryValue sub_dictionary2;
  sub_dictionary2.Set(sub_key2, pref2.CreateDeepCopy());

  base::DictionaryValue prefs;
  prefs.Set(key1, sub_dictionary1.CreateDeepCopy());
  prefs.Set(key2, sub_dictionary2.CreateDeepCopy());

  // Initialize with the nested dictionaries
  OnPreferencesChanged(prefs);
  EXPECT_TRUE(Initialized());
  EXPECT_TRUE(observer()->initialized);
  EXPECT_EQ(2u, observer()->changed_keys.size());
  EXPECT_NE(observer()->changed_keys.end(),
            std::find(observer()->changed_keys.begin(),
                      observer()->changed_keys.end(), key1));
  EXPECT_NE(observer()->changed_keys.end(),
            std::find(observer()->changed_keys.begin(),
                      observer()->changed_keys.end(), key2));

  // Change an item within the nested dictionary
  base::Value* result = nullptr;
  store()->GetMutableValue(key1, &result);
  EXPECT_EQ(base::Value::Type::DICTIONARY, result->GetType());
  EXPECT_TRUE(result->Equals(&sub_dictionary1));

  base::DictionaryValue* dictionary_result = nullptr;
  result->GetAsDictionary(&dictionary_result);
  EXPECT_NE(nullptr, dictionary_result);

  const std::string sub_key3("????");
  const int kValue3 = 9001;
  base::Value pref3(kValue3);
  dictionary_result->Set(sub_key3, pref3.CreateDeepCopy());

  observer()->changed_keys.clear();
  store()->ReportValueChanged(key1, 0);
  EXPECT_EQ(1u, observer()->changed_keys.size());
}

// Tests that when tracking preferences that nest other DictionaryValues, that
// changes to the tracked keys properly notify the service and observer.
TEST_F(PrefClientStoreTest, UpdateOuterNestedPrefs) {
  std::set<std::string> keys;
  const std::string key1("hey");
  const std::string key2("listen");
  keys.insert(key1);
  keys.insert(key2);
  store()->Subscribe(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const std::string sub_key1("look");
  const int kValue1 = 42;
  base::Value pref1(kValue1);
  base::DictionaryValue sub_dictionary1;
  sub_dictionary1.Set(sub_key1, pref1.CreateDeepCopy());

  const std::string sub_key2("Watch out!\n");
  const int kValue2 = 1337;
  base::Value pref2(kValue2);
  base::DictionaryValue sub_dictionary2;
  sub_dictionary2.Set(sub_key2, pref2.CreateDeepCopy());

  base::DictionaryValue prefs;
  prefs.Set(key1, sub_dictionary1.CreateDeepCopy());
  prefs.Set(key2, sub_dictionary2.CreateDeepCopy());

  // Initialize with the nested dictionaries
  OnPreferencesChanged(prefs);
  EXPECT_TRUE(Initialized());
  EXPECT_TRUE(observer()->initialized);
  EXPECT_EQ(2u, observer()->changed_keys.size());
  EXPECT_NE(observer()->changed_keys.end(),
            std::find(observer()->changed_keys.begin(),
                      observer()->changed_keys.end(), key1));
  EXPECT_NE(observer()->changed_keys.end(),
            std::find(observer()->changed_keys.begin(),
                      observer()->changed_keys.end(), key2));

  observer()->changed_keys.clear();
  const int kValue3 = 9001;
  base::Value pref3(kValue3);
  store()->SetValue(key1, pref3.CreateDeepCopy(), 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, observer()->changed_keys.size());
  EXPECT_TRUE(service()->set_preferences_called());
}

// Tests that a PrefClientStore can subscribe multiple times to different
// keys.
TEST_F(PrefClientStoreTest, MultipleSubscriptions) {
  std::set<std::string> keys1;
  const std::string key1("hey");
  keys1.insert(key1);
  store()->Subscribe(keys1);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(service()->last_preference_set().end(),
            service()->last_preference_set().find(key1));

  std::set<std::string> keys2;
  const std::string key2("listen");
  keys2.insert(key2);
  store()->Subscribe(keys2);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(service()->last_preference_set().end(),
            service()->last_preference_set().find(key2));
}

// Tests that multiple PrefStore::Observers can be added to a PrefClientStore
// and that they are each notified of changes.
TEST_F(PrefClientStoreTest, MultipleObservers) {
  PrefStoreObserverMock observer2;
  store()->AddObserver(&observer2);

  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Subscribe(keys);

  const int kValue = 42;
  base::Value pref(kValue);
  base::DictionaryValue prefs;
  prefs.Set(key, pref.CreateDeepCopy());

  // PreferenceManager notifies of PreferencesChanged, completing
  // initialization.
  OnPreferencesChanged(prefs);
  EXPECT_TRUE(observer()->initialized);
  EXPECT_TRUE(observer2.initialized);
  EXPECT_TRUE(observer()->initialization_success);
  EXPECT_TRUE(observer2.initialization_success);
  observer()->VerifyAndResetChangedKey(key);
  observer2.VerifyAndResetChangedKey(key);

  store()->RemoveObserver(&observer2);
}

}  // namespace preferences
