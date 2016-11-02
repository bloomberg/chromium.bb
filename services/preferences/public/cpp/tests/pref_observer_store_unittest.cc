// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_observer_store.h"

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
class TestPreferenceManager : public prefs::mojom::PreferencesManager {
 public:
  TestPreferenceManager(
      mojo::InterfaceRequest<prefs::mojom::PreferencesManager> request)
      : add_observer_called_(false),
        set_preferences_called_(false),
        binding_(this, std::move(request)) {}
  ~TestPreferenceManager() override {}

  bool add_observer_called() { return add_observer_called_; }
  const std::set<std::string>& last_preference_set() {
    return last_preference_set_;
  }
  bool set_preferences_called() { return set_preferences_called_; }

  // prefs::mojom::TestPreferenceManager:
  void AddObserver(const std::vector<std::string>& preferences,
                   prefs::mojom::PreferencesObserverPtr client) override;
  void SetPreferences(const base::DictionaryValue& preferences) override;

 private:
  bool add_observer_called_;
  std::set<std::string> last_preference_set_;
  bool set_preferences_called_;
  mojo::Binding<PreferencesManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestPreferenceManager);
};

void TestPreferenceManager::AddObserver(
    const std::vector<std::string>& preferences,
    prefs::mojom::PreferencesObserverPtr client) {
  add_observer_called_ = true;
  last_preference_set_.clear();
  last_preference_set_.insert(preferences.begin(), preferences.end());
}

void TestPreferenceManager::SetPreferences(
    const base::DictionaryValue& preferences) {
  set_preferences_called_ = true;
}

}  // namespace

class PrefObserverStoreTest : public testing::Test {
 public:
  PrefObserverStoreTest() {}
  ~PrefObserverStoreTest() override {}

  TestPreferenceManager* manager() { return manager_.get(); }
  PrefStoreObserverMock* observer() { return &observer_; }
  PrefObserverStore* store() { return store_.get(); }

  bool Initialized() { return store_->initialized_; }
  void OnPreferencesChanged(const base::DictionaryValue& preferences) {
    store_->OnPreferencesChanged(std::move(preferences));
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  scoped_refptr<PrefObserverStore> store_;
  prefs::mojom::PreferencesManagerPtr proxy_;
  std::unique_ptr<TestPreferenceManager> manager_;
  PrefStoreObserverMock observer_;
  // Required by mojo binding code within PrefObserverStore.
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PrefObserverStoreTest);
};

void PrefObserverStoreTest::SetUp() {
  manager_.reset(new TestPreferenceManager(mojo::GetProxy(&proxy_)));
  store_ = new PrefObserverStore(std::move(proxy_));
  store_->AddObserver(&observer_);
}

void PrefObserverStoreTest::TearDown() {
  store_->RemoveObserver(&observer_);
}

// Tests that observers are notified upon the completion of initialization, and
// that values become available.
TEST_F(PrefObserverStoreTest, Initialization) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Init(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const int kValue = 42;
  base::FundamentalValue pref(kValue);
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
  EXPECT_FALSE(manager()->set_preferences_called());
}

// Tests that values set silently are also set on the preference manager, but
// that no observers are notified.
TEST_F(PrefObserverStoreTest, SetValueSilently) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Init(keys);

  const int kValue = 42;
  base::FundamentalValue pref(kValue);
  store()->SetValueSilently(key, pref.CreateDeepCopy(), 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(manager()->set_preferences_called());
  EXPECT_TRUE(observer()->changed_keys.empty());
}

// Test that reporting values changed notifies observers, and the preference
// manager.
TEST_F(PrefObserverStoreTest, ReportValueChanged) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Init(keys);

  const int kValue = 42;
  base::FundamentalValue pref(kValue);
  base::DictionaryValue prefs;
  prefs.Set(key, pref.CreateDeepCopy());
  OnPreferencesChanged(prefs);
  observer()->changed_keys.clear();

  store()->ReportValueChanged(key, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(manager()->set_preferences_called());
  observer()->VerifyAndResetChangedKey(key);
}

// Test that when initialized with multiple keys, that observers receive a
// notification for each key.
TEST_F(PrefObserverStoreTest, MultipleKeyInitialization) {
  std::set<std::string> keys;
  const std::string key1("hey");
  const std::string key2("listen");
  keys.insert(key1);
  keys.insert(key2);
  store()->Init(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const int kValue = 42;
  base::FundamentalValue pref1(kValue);
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
TEST_F(PrefObserverStoreTest, InvalidInitialization) {
  std::set<std::string> keys;
  const std::string key("hey");
  keys.insert(key);
  store()->Init(keys);

  const std::string kInvalidKey("look");
  const int kValue = 42;
  base::FundamentalValue pref(kValue);
  base::DictionaryValue prefs;
  prefs.Set(kInvalidKey, pref.CreateDeepCopy());

  OnPreferencesChanged(prefs);
  EXPECT_TRUE(observer()->changed_keys.empty());
}

// Tests that when tracking preferences which nest other DictionaryValues, that
// modifications to the nested values properly notify the observer.
TEST_F(PrefObserverStoreTest, WriteToNestedPrefs) {
  std::set<std::string> keys;
  const std::string key1("hey");
  const std::string key2("listen");
  keys.insert(key1);
  keys.insert(key2);
  store()->Init(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const std::string sub_key1("look");
  const int kValue1 = 42;
  base::FundamentalValue pref1(kValue1);
  base::DictionaryValue sub_dictionary1;
  sub_dictionary1.Set(sub_key1, pref1.CreateDeepCopy());

  const std::string sub_key2("Watch out!\n");
  const int kValue2 = 1337;
  base::FundamentalValue pref2(kValue2);
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
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  EXPECT_TRUE(result->Equals(&sub_dictionary1));

  base::DictionaryValue* dictionary_result = nullptr;
  result->GetAsDictionary(&dictionary_result);
  EXPECT_NE(nullptr, dictionary_result);

  const std::string sub_key3("????");
  const int kValue3 = 9001;
  base::FundamentalValue pref3(kValue3);
  dictionary_result->Set(sub_key3, pref3.CreateDeepCopy());

  observer()->changed_keys.clear();
  store()->ReportValueChanged(key1, 0);
  EXPECT_EQ(1u, observer()->changed_keys.size());
}

// Tests that when tracking preferences that nest other DictionaryValues, that
// changes to the tracked keys properly notify the manager and observer.
TEST_F(PrefObserverStoreTest, UpdateOuterNestedPrefs) {
  std::set<std::string> keys;
  const std::string key1("hey");
  const std::string key2("listen");
  keys.insert(key1);
  keys.insert(key2);
  store()->Init(keys);

  EXPECT_FALSE(Initialized());
  EXPECT_FALSE(observer()->initialized);

  const std::string sub_key1("look");
  const int kValue1 = 42;
  base::FundamentalValue pref1(kValue1);
  base::DictionaryValue sub_dictionary1;
  sub_dictionary1.Set(sub_key1, pref1.CreateDeepCopy());

  const std::string sub_key2("Watch out!\n");
  const int kValue2 = 1337;
  base::FundamentalValue pref2(kValue2);
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
  base::FundamentalValue pref3(kValue3);
  store()->SetValue(key1, pref3.CreateDeepCopy(), 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, observer()->changed_keys.size());
  EXPECT_TRUE(manager()->set_preferences_called());
}
