// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_service_factory.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/value_map_pref_store.h"
#include "components/prefs/writeable_pref_store.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/pref_service_main.h"
#include "services/preferences/public/cpp/pref_store_impl.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"

namespace prefs {
namespace {

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  ServiceTestClient(service_manager::test::ServiceTest* test,
                    scoped_refptr<base::SequencedWorkerPool> worker_pool)
      : service_manager::test::ServiceTestClient(test),
        worker_pool_(std::move(worker_pool)) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::Bind(&ServiceTestClient::Create, base::Unretained(this)));
  }

 protected:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(source_info, interface_name,
                            std::move(interface_pipe));
  }

  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == prefs::mojom::kServiceName) {
      pref_service_context_.reset(new service_manager::ServiceContext(
          CreatePrefService(
              {PrefValueStore::COMMAND_LINE_STORE,
               PrefValueStore::RECOMMENDED_STORE, PrefValueStore::USER_STORE,
               PrefValueStore::DEFAULT_STORE},
              worker_pool_),
          std::move(request)));
    }
  }

  void Create(const service_manager::BindSourceInfo& source_info,
              service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> pref_service_context_;
};

constexpr int kInitialValue = 1;
constexpr int kUpdatedValue = 2;
constexpr char kKey[] = "some_key";
constexpr char kOtherKey[] = "some_other_key";
constexpr char kDictionaryKey[] = "a.dictionary.pref";

class PrefServiceFactoryTest : public service_manager::test::ServiceTest {
 public:
  PrefServiceFactoryTest()
      : ServiceTest("prefs_unittests", false),
        worker_pool_owner_(2, "PrefServiceFactoryTest") {}

 protected:
  void SetUp() override {
    ServiceTest::SetUp();
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());

    // Init the pref service (in production Chrome startup would do this.)
    mojom::PrefServiceControlPtr control;
    connector()->BindInterface(mojom::kServiceName, &control);
    auto config = mojom::PersistentPrefStoreConfiguration::New();
    config->set_simple_configuration(
        mojom::SimplePersistentPrefStoreConfiguration::New(
            profile_dir_.GetPath().AppendASCII("Preferences")));
    control->Init(std::move(config));
    above_user_prefs_pref_store_ = new ValueMapPrefStore();
    below_user_prefs_pref_store_ = new ValueMapPrefStore();
    mojom::PrefStoreRegistryPtr registry;
    connector()->BindInterface(mojom::kServiceName, &registry);
    above_user_prefs_impl_ =
        PrefStoreImpl::Create(registry.get(), above_user_prefs_pref_store_,
                              PrefValueStore::COMMAND_LINE_STORE);
    below_user_prefs_impl_ =
        PrefStoreImpl::Create(registry.get(), below_user_prefs_pref_store_,
                              PrefValueStore::RECOMMENDED_STORE);
  }

  // service_manager::test::ServiceTest:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return base::MakeUnique<ServiceTestClient>(this, worker_pool_owner_.pool());
  }

  // Create a fully initialized PrefService synchronously.
  std::unique_ptr<PrefService> Create() {
    std::unique_ptr<PrefService> pref_service;
    base::RunLoop run_loop;
    auto pref_registry = make_scoped_refptr(new PrefRegistrySimple());
    pref_registry->RegisterIntegerPref(kKey, kInitialValue);
    pref_registry->RegisterIntegerPref(kOtherKey, kInitialValue);
    pref_registry->RegisterDictionaryPref(kDictionaryKey);
    ConnectToPrefService(connector(), pref_registry,
                         std::vector<PrefValueStore::PrefStoreType>(),
                         base::Bind(&PrefServiceFactoryTest::OnCreate,
                                    run_loop.QuitClosure(), &pref_service));
    run_loop.Run();
    return pref_service;
  }

  // Wait until first update of the pref |key| in |pref_service| synchronously.
  void WaitForPrefChange(PrefService* pref_service, const std::string& key) {
    PrefChangeRegistrar registrar;
    registrar.Init(pref_service);
    base::RunLoop run_loop;
    registrar.Add(key, base::Bind(&OnPrefChanged, run_loop.QuitClosure(), key));
    run_loop.Run();
  }

  WriteablePrefStore* above_user_prefs_pref_store() {
    return above_user_prefs_pref_store_.get();
  }
  WriteablePrefStore* below_user_prefs_pref_store() {
    return below_user_prefs_pref_store_.get();
  }

 private:
  // Called when the PrefService has been initialized.
  static void OnInit(const base::Closure& quit_closure, bool success) {
    quit_closure.Run();
  }

  // Called when the PrefService has been created.
  static void OnCreate(const base::Closure& quit_closure,
                       std::unique_ptr<PrefService>* out,
                       std::unique_ptr<PrefService> pref_service) {
    DCHECK(pref_service);
    *out = std::move(pref_service);
    if ((*out)->GetInitializationStatus() ==
        PrefService::INITIALIZATION_STATUS_WAITING) {
      (*out)->AddPrefInitObserver(
          base::Bind(PrefServiceFactoryTest::OnInit, quit_closure));
      return;
    }
    quit_closure.Run();
  }

  static void OnPrefChanged(const base::Closure& quit_closure,
                            const std::string& expected_path,
                            const std::string& path) {
    if (path == expected_path)
      quit_closure.Run();
  }

  base::ScopedTempDir profile_dir_;
  base::SequencedWorkerPoolOwner worker_pool_owner_;
  scoped_refptr<WriteablePrefStore> above_user_prefs_pref_store_;
  std::unique_ptr<PrefStoreImpl> above_user_prefs_impl_;
  scoped_refptr<WriteablePrefStore> below_user_prefs_pref_store_;
  std::unique_ptr<PrefStoreImpl> below_user_prefs_impl_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceFactoryTest);
};

// Check that a single client can set and read back values.
TEST_F(PrefServiceFactoryTest, Basic) {
  auto pref_service = Create();

  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));
  pref_service->SetInteger(kKey, kUpdatedValue);
  EXPECT_EQ(kUpdatedValue, pref_service->GetInteger(kKey));
}

// Check that updates in one client eventually propagates to the other.
TEST_F(PrefServiceFactoryTest, MultipleClients) {
  auto pref_service = Create();
  auto pref_service2 = Create();

  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));
  EXPECT_EQ(kInitialValue, pref_service2->GetInteger(kKey));
  pref_service->SetInteger(kKey, kUpdatedValue);
  WaitForPrefChange(pref_service2.get(), kKey);
  EXPECT_EQ(kUpdatedValue, pref_service2->GetInteger(kKey));
}

// Check that read-only pref store changes are observed.
TEST_F(PrefServiceFactoryTest, ReadOnlyPrefStore) {
  auto pref_service = Create();

  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));

  below_user_prefs_pref_store()->SetValue(
      kKey, base::MakeUnique<base::Value>(kUpdatedValue), 0);
  WaitForPrefChange(pref_service.get(), kKey);
  EXPECT_EQ(kUpdatedValue, pref_service->GetInteger(kKey));
  pref_service->SetInteger(kKey, 3);
  EXPECT_EQ(3, pref_service->GetInteger(kKey));
  above_user_prefs_pref_store()->SetValue(kKey,
                                          base::MakeUnique<base::Value>(4), 0);
  WaitForPrefChange(pref_service.get(), kKey);
  EXPECT_EQ(4, pref_service->GetInteger(kKey));
}

// Check that updates to read-only pref stores are correctly layered.
TEST_F(PrefServiceFactoryTest, ReadOnlyPrefStore_Layering) {
  auto pref_service = Create();

  above_user_prefs_pref_store()->SetValue(
      kKey, base::MakeUnique<base::Value>(kInitialValue), 0);
  WaitForPrefChange(pref_service.get(), kKey);
  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));

  below_user_prefs_pref_store()->SetValue(
      kKey, base::MakeUnique<base::Value>(kUpdatedValue), 0);
  // This update is needed to check that the change to kKey has propagated even
  // though we will not observe it change.
  below_user_prefs_pref_store()->SetValue(
      kOtherKey, base::MakeUnique<base::Value>(kUpdatedValue), 0);
  WaitForPrefChange(pref_service.get(), kOtherKey);
  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));
}

// Check that writes to user prefs are correctly layered with read-only
// pref stores.
TEST_F(PrefServiceFactoryTest, ReadOnlyPrefStore_UserPrefStoreLayering) {
  auto pref_service = Create();

  above_user_prefs_pref_store()->SetValue(kKey,
                                          base::MakeUnique<base::Value>(2), 0);
  WaitForPrefChange(pref_service.get(), kKey);
  EXPECT_EQ(2, pref_service->GetInteger(kKey));

  pref_service->SetInteger(kKey, 3);
  EXPECT_EQ(2, pref_service->GetInteger(kKey));
}

void Fail(PrefService* pref_service) {
  FAIL() << "Unexpected change notification: "
         << *pref_service->GetDictionary(kDictionaryKey);
}

TEST_F(PrefServiceFactoryTest, MultipleClients_SubPrefUpdates_Basic) {
  auto pref_service = Create();
  auto pref_service2 = Create();

  void (*updates[])(ScopedDictionaryPrefUpdate*) = {
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetInteger("path.to.integer", 1);
        int out = 0;
        ASSERT_TRUE((*update)->GetInteger("path.to.integer", &out));
        EXPECT_EQ(1, out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetIntegerWithoutPathExpansion("key.for.integer", 2);
        int out = 0;
        ASSERT_TRUE(
            (*update)->GetIntegerWithoutPathExpansion("key.for.integer", &out));
        EXPECT_EQ(2, out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetDouble("path.to.double", 3);
        double out = 0;
        ASSERT_TRUE((*update)->GetDouble("path.to.double", &out));
        EXPECT_EQ(3, out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetDoubleWithoutPathExpansion("key.for.double", 4);
        double out = 0;
        ASSERT_TRUE(
            (*update)->GetDoubleWithoutPathExpansion("key.for.double", &out));
        EXPECT_EQ(4, out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetBoolean("path.to.boolean", true);
        bool out = 0;
        ASSERT_TRUE((*update)->GetBoolean("path.to.boolean", &out));
        EXPECT_TRUE(out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetBooleanWithoutPathExpansion("key.for.boolean", false);
        bool out = 0;
        ASSERT_TRUE(
            (*update)->GetBooleanWithoutPathExpansion("key.for.boolean", &out));
        EXPECT_FALSE(out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetString("path.to.string", "hello");
        std::string out;
        ASSERT_TRUE((*update)->GetString("path.to.string", &out));
        EXPECT_EQ("hello", out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetStringWithoutPathExpansion("key.for.string", "prefs!");
        std::string out;
        ASSERT_TRUE(
            (*update)->GetStringWithoutPathExpansion("key.for.string", &out));
        EXPECT_EQ("prefs!", out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetString("path.to.string16", base::ASCIIToUTF16("hello"));
        base::string16 out;
        ASSERT_TRUE((*update)->GetString("path.to.string16", &out));
        EXPECT_EQ(base::ASCIIToUTF16("hello"), out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        (*update)->SetStringWithoutPathExpansion("key.for.string16",
                                                 base::ASCIIToUTF16("prefs!"));
        base::string16 out;
        ASSERT_TRUE(
            (*update)->GetStringWithoutPathExpansion("key.for.string16", &out));
        EXPECT_EQ(base::ASCIIToUTF16("prefs!"), out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        base::ListValue list;
        list.AppendInteger(1);
        list.AppendDouble(2);
        list.AppendBoolean(true);
        list.AppendString("four");
        (*update)->Set("path.to.list", list.CreateDeepCopy());
        const base::ListValue* out = nullptr;
        ASSERT_TRUE((*update)->GetList("path.to.list", &out));
        EXPECT_EQ(list, *out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        base::ListValue list;
        list.AppendInteger(1);
        list.AppendDouble(2);
        list.AppendBoolean(true);
        list.AppendString("four");
        (*update)->SetWithoutPathExpansion("key.for.list",
                                           list.CreateDeepCopy());
        const base::ListValue* out = nullptr;
        ASSERT_TRUE(
            (*update)->GetListWithoutPathExpansion("key.for.list", &out));
        EXPECT_EQ(list, *out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        base::DictionaryValue dict;
        dict.SetInteger("int", 1);
        dict.SetDouble("double", 2);
        dict.SetBoolean("bool", true);
        dict.SetString("string", "four");
        (*update)->Set("path.to.dict", dict.CreateDeepCopy());
        const base::DictionaryValue* out = nullptr;
        ASSERT_TRUE((*update)->GetDictionary("path.to.dict", &out));
        EXPECT_EQ(dict, *out);
      },
      [](ScopedDictionaryPrefUpdate* update) {
        base::DictionaryValue dict;
        dict.SetInteger("int", 1);
        dict.SetDouble("double", 2);
        dict.SetBoolean("bool", true);
        dict.SetString("string", "four");
        (*update)->SetWithoutPathExpansion("key.for.dict",
                                           dict.CreateDeepCopy());
        const base::DictionaryValue* out = nullptr;
        ASSERT_TRUE(
            (*update)->GetDictionaryWithoutPathExpansion("key.for.dict", &out));
        EXPECT_EQ(dict, *out);
      },
  };
  int current_value = kInitialValue + 1;
  for (auto& mutation : updates) {
    base::DictionaryValue expected_value;
    {
      ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
      EXPECT_EQ(update->AsConstDictionary()->empty(), update->empty());
      EXPECT_EQ(update->AsConstDictionary()->size(), update->size());
      mutation(&update);
      EXPECT_EQ(update->AsConstDictionary()->empty(), update->empty());
      EXPECT_EQ(update->AsConstDictionary()->size(), update->size());
      expected_value = *update->AsConstDictionary();
    }

    EXPECT_EQ(expected_value, *pref_service->GetDictionary(kDictionaryKey));
    WaitForPrefChange(pref_service2.get(), kDictionaryKey);
    EXPECT_EQ(expected_value, *pref_service2->GetDictionary(kDictionaryKey));

    {
      // Apply the same mutation again. Each mutation should be idempotent so
      // should not trigger a notification.
      ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
      mutation(&update);
      EXPECT_EQ(expected_value, *update->AsConstDictionary());
    }
    {
      // Watch for an unexpected change to kDictionaryKey.
      PrefChangeRegistrar registrar;
      registrar.Init(pref_service2.get());
      registrar.Add(kDictionaryKey, base::Bind(&Fail, pref_service2.get()));

      // Make and wait for a change to another pref to ensure an unexpected
      // change to kDictionaryKey is detected.
      pref_service->SetInteger(kKey, ++current_value);
      WaitForPrefChange(pref_service2.get(), kKey);
    }
  }
}

TEST_F(PrefServiceFactoryTest, MultipleClients_SubPrefUpdates_Erase) {
  auto pref_service = Create();
  auto pref_service2 = Create();
  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForPrefChange(pref_service2.get(), kDictionaryKey);
  EXPECT_FALSE(pref_service2->GetDictionary(kDictionaryKey)->empty());

  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    ASSERT_TRUE(update->RemovePath("path.to.integer", nullptr));
  }
  WaitForPrefChange(pref_service2.get(), kDictionaryKey);
  EXPECT_TRUE(pref_service2->GetDictionary(kDictionaryKey)->empty());
}

TEST_F(PrefServiceFactoryTest, MultipleClients_SubPrefUpdates_ClearDictionary) {
  auto pref_service = Create();
  auto pref_service2 = Create();

  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForPrefChange(pref_service2.get(), kDictionaryKey);
  EXPECT_FALSE(pref_service2->GetDictionary(kDictionaryKey)->empty());

  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    update->Clear();
  }
  WaitForPrefChange(pref_service2.get(), kDictionaryKey);
  EXPECT_TRUE(pref_service2->GetDictionary(kDictionaryKey)->empty());
}

TEST_F(PrefServiceFactoryTest,
       MultipleClients_SubPrefUpdates_ClearEmptyDictionary) {
  auto pref_service = Create();
  auto pref_service2 = Create();

  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    update.Get();
  }
  WaitForPrefChange(pref_service2.get(), kDictionaryKey);
  EXPECT_TRUE(pref_service2->GetDictionary(kDictionaryKey)->empty());

  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    update->Clear();
  }
  PrefChangeRegistrar registrar;
  registrar.Init(pref_service2.get());
  registrar.Add(kDictionaryKey, base::Bind(&Fail, pref_service2.get()));
  pref_service->SetInteger(kKey, kUpdatedValue);
  WaitForPrefChange(pref_service2.get(), kKey);
}

}  // namespace
}  // namespace prefs
