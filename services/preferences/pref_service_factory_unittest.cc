// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_service_factory.h"

#include "base/barrier_closure.h"
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
  ServiceTestClient(
      service_manager::test::ServiceTest* test,
      scoped_refptr<base::SequencedWorkerPool> worker_pool,
      base::OnceCallback<void(service_manager::Connector*)> connector_callback)
      : service_manager::test::ServiceTestClient(test),
        worker_pool_(std::move(worker_pool)),
        connector_callback_(std::move(connector_callback)) {
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
    } else if (name == "prefs_unittest_helper") {
      test_helper_service_context_ =
          base::MakeUnique<service_manager::ServiceContext>(
              base::MakeUnique<service_manager::Service>(), std::move(request));
      std::move(connector_callback_)
          .Run(test_helper_service_context_->connector());
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
  std::unique_ptr<service_manager::ServiceContext> test_helper_service_context_;
  base::OnceCallback<void(service_manager::Connector*)> connector_callback_;
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
    base::RunLoop run_loop;
    connector_callback_ =
        base::BindOnce(&PrefServiceFactoryTest::SetOtherClientConnector,
                       base::Unretained(this), run_loop.QuitClosure());
    ServiceTest::SetUp();
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    connector()->StartService("prefs_unittest_helper");
    run_loop.Run();

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
    connector()->BindInterface(mojom::kServiceName, &pref_store_registry_);
    CreateLayeredPrefStores();
  }

  service_manager::Connector* other_client_connector() {
    return other_client_connector_;
  }

  virtual void CreateLayeredPrefStores() {
    above_user_prefs_impl_ = PrefStoreImpl::Create(
        pref_store_registry(), above_user_prefs_pref_store_,
        PrefValueStore::COMMAND_LINE_STORE);
    below_user_prefs_impl_ = PrefStoreImpl::Create(
        pref_store_registry(), below_user_prefs_pref_store_,
        PrefValueStore::RECOMMENDED_STORE);
  }

  // service_manager::test::ServiceTest:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return base::MakeUnique<ServiceTestClient>(this, worker_pool_owner_.pool(),
                                               std::move(connector_callback_));
  }

  // Create a fully initialized PrefService synchronously.
  std::unique_ptr<PrefService> Create() {
    return CreateImpl(CreateDefaultPrefRegistry(), {}, connector());
  }

  std::unique_ptr<PrefService> CreateForeign() {
    return CreateImpl(CreateDefaultForeignPrefRegistry(), {},
                      other_client_connector());
  }

  std::unique_ptr<PrefService> CreateWithLocalLayeredPrefStores() {
    return CreateImpl(
        CreateDefaultPrefRegistry(),
        {{
             {PrefValueStore::COMMAND_LINE_STORE, above_user_prefs_pref_store_},
             {PrefValueStore::RECOMMENDED_STORE, below_user_prefs_pref_store_},
         },
         base::KEEP_LAST_OF_DUPES},
        connector());
  }

  std::unique_ptr<PrefService> CreateImpl(
      scoped_refptr<PrefRegistry> pref_registry,
      base::flat_map<PrefValueStore::PrefStoreType, scoped_refptr<PrefStore>>
          local_pref_stores,
      service_manager::Connector* connector) {
    std::unique_ptr<PrefService> pref_service;
    base::RunLoop run_loop;
    CreateAsync(std::move(pref_registry), std::move(local_pref_stores),
                connector, run_loop.QuitClosure(), &pref_service);
    run_loop.Run();
    return pref_service;
  }

  void CreateAsync(scoped_refptr<PrefRegistry> pref_registry,
                   base::flat_map<PrefValueStore::PrefStoreType,
                                  scoped_refptr<PrefStore>> local_pref_stores,
                   service_manager::Connector* connector,
                   base::Closure callback,
                   std::unique_ptr<PrefService>* out) {
    ConnectToPrefService(
        connector, std::move(pref_registry), std::move(local_pref_stores),
        base::Bind(&PrefServiceFactoryTest::OnCreate, callback, out));
  }

  scoped_refptr<PrefRegistrySimple> CreateDefaultPrefRegistry() {
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    pref_registry->RegisterIntegerPref(kKey, kInitialValue,
                                       PrefRegistry::PUBLIC);
    pref_registry->RegisterIntegerPref(kOtherKey, kInitialValue,
                                       PrefRegistry::PUBLIC);
    pref_registry->RegisterDictionaryPref(kDictionaryKey, PrefRegistry::PUBLIC);
    return pref_registry;
  }

  scoped_refptr<PrefRegistrySimple> CreateDefaultForeignPrefRegistry() {
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    pref_registry->RegisterForeignPref(kKey);
    pref_registry->RegisterForeignPref(kOtherKey);
    pref_registry->RegisterForeignPref(kDictionaryKey);
    return pref_registry;
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
  mojom::PrefStoreRegistry* pref_store_registry() {
    return pref_store_registry_.get();
  }

 private:
  void SetOtherClientConnector(base::OnceClosure done,
                               service_manager::Connector* connector) {
    other_client_connector_ = connector;
    std::move(done).Run();
  }

  // Called when the PrefService has been created.
  static void OnCreate(const base::Closure& quit_closure,
                       std::unique_ptr<PrefService>* out,
                       std::unique_ptr<PrefService> pref_service) {
    DCHECK(pref_service);
    *out = std::move(pref_service);
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
  mojom::PrefStoreRegistryPtr pref_store_registry_;
  service_manager::Connector* other_client_connector_ = nullptr;
  base::OnceCallback<void(service_manager::Connector*)> connector_callback_;

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
  auto pref_service2 = CreateForeign();

  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));
  EXPECT_EQ(kInitialValue, pref_service2->GetInteger(kKey));
  pref_service->SetInteger(kKey, kUpdatedValue);
  WaitForPrefChange(pref_service2.get(), kKey);
  EXPECT_EQ(kUpdatedValue, pref_service2->GetInteger(kKey));
}

TEST_F(PrefServiceFactoryTest, MultipleConnectionsFromSingleClient) {
  Create();
  CreateForeign();
  Create();
  CreateForeign();
}

// Check that defaults set by one client are correctly shared to the other
// client.
TEST_F(PrefServiceFactoryTest, MultipleClients_Defaults) {
  std::unique_ptr<PrefService> pref_service, pref_service2;
  {
    base::RunLoop run_loop;
    auto done_closure = base::BarrierClosure(2, run_loop.QuitClosure());

    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    pref_registry->RegisterIntegerPref(kKey, kInitialValue,
                                       PrefRegistry::PUBLIC);
    pref_registry->RegisterForeignPref(kOtherKey);
    auto pref_registry2 = base::MakeRefCounted<PrefRegistrySimple>();
    pref_registry2->RegisterForeignPref(kKey);
    pref_registry2->RegisterIntegerPref(kOtherKey, kInitialValue,
                                        PrefRegistry::PUBLIC);
    CreateAsync(std::move(pref_registry), {}, connector(), done_closure,
                &pref_service);
    CreateAsync(std::move(pref_registry2), {}, other_client_connector(),
                done_closure, &pref_service2);
    run_loop.Run();
  }

  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));
  EXPECT_EQ(kInitialValue, pref_service2->GetInteger(kKey));
  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kOtherKey));
  EXPECT_EQ(kInitialValue, pref_service2->GetInteger(kOtherKey));
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

// Check that local read-only pref store changes are observed.
TEST_F(PrefServiceFactoryTest, ReadOnlyPrefStore_Local) {
  auto pref_service = CreateWithLocalLayeredPrefStores();

  EXPECT_EQ(kInitialValue, pref_service->GetInteger(kKey));

  below_user_prefs_pref_store()->SetValue(
      kKey, base::MakeUnique<base::Value>(kUpdatedValue), 0);
  EXPECT_EQ(kUpdatedValue, pref_service->GetInteger(kKey));
  pref_service->SetInteger(kKey, 3);
  EXPECT_EQ(3, pref_service->GetInteger(kKey));
  above_user_prefs_pref_store()->SetValue(kKey,
                                          base::MakeUnique<base::Value>(4), 0);
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
  auto pref_service2 = CreateForeign();

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
  auto pref_service2 = CreateForeign();
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
  auto pref_service2 = CreateForeign();

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
  auto pref_service2 = CreateForeign();

  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    update->SetInteger(kKey, kInitialValue);
  }
  WaitForPrefChange(pref_service2.get(), kDictionaryKey);
  {
    ScopedDictionaryPrefUpdate update(pref_service.get(), kDictionaryKey);
    update->Remove(kKey, nullptr);
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

class PrefServiceFactoryManualPrefStoreRegistrationTest
    : public PrefServiceFactoryTest {
 protected:
  void CreateLayeredPrefStores() override {}
};

class ConnectionReportingPrefStoreImpl : public mojom::PrefStore {
 public:
  ConnectionReportingPrefStoreImpl(base::OnceClosure on_add_observer,
                                   mojom::PrefStoreRequest request)
      : on_add_observer_(std::move(on_add_observer)),
        binding_(this, std::move(request)) {}

  void AddObserver(const std::vector<std::string>& prefs_to_observe,
                   AddObserverCallback callback) override {
    observer_added_ = true;
    if (on_add_observer_)
      std::move(on_add_observer_).Run();

    mojom::PrefStoreObserverPtr observer;
    auto values = base::MakeUnique<base::DictionaryValue>();
    values->Set(kKey, base::MakeUnique<base::Value>(kUpdatedValue));
    std::move(callback).Run(mojom::PrefStoreConnection::New(
        mojo::MakeRequest(&observer), std::move(values), true));
  }

  bool observer_added() { return observer_added_; }

 private:
  bool observer_added_ = false;
  base::OnceClosure on_add_observer_;
  mojo::Binding<mojom::PrefStore> binding_;
};

// Check that connect calls received before all read-only prefs stores have
// been registered are correctly handled.
TEST_F(PrefServiceFactoryManualPrefStoreRegistrationTest,
       RegistrationBeforeAndAfterConnect) {
  base::RunLoop add_observer_run_loop;

  mojom::PrefStorePtr below_ptr;
  ConnectionReportingPrefStoreImpl below_user_prefs(
      add_observer_run_loop.QuitClosure(), mojo::MakeRequest(&below_ptr));
  pref_store_registry()->Register(PrefValueStore::RECOMMENDED_STORE,
                                  std::move(below_ptr));

  std::unique_ptr<PrefService> pref_service;

  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());
  CreateAsync(CreateDefaultPrefRegistry(), {}, connector(), barrier,
              &pref_service);

  add_observer_run_loop.Run();
  ASSERT_TRUE(below_user_prefs.observer_added());

  EXPECT_FALSE(pref_service);

  mojom::PrefStorePtr above_ptr;
  ConnectionReportingPrefStoreImpl above_user_prefs(
      barrier, mojo::MakeRequest(&above_ptr));
  pref_store_registry()->Register(PrefValueStore::COMMAND_LINE_STORE,
                                  std::move(above_ptr));

  run_loop.Run();
  EXPECT_TRUE(above_user_prefs.observer_added());
  EXPECT_EQ(kUpdatedValue, pref_service->GetInteger(kKey));
  pref_service->SetInteger(kKey, kInitialValue);
  EXPECT_EQ(kUpdatedValue, pref_service->GetInteger(kKey));
}

// Check that connect calls claiming to have all read-only pref stores locally
// do not wait for those stores to be registered with the pref service.
TEST_F(PrefServiceFactoryManualPrefStoreRegistrationTest,
       LocalButNotRegisteredReadOnlyStores) {
  EXPECT_TRUE(CreateWithLocalLayeredPrefStores());
}

}  // namespace
}  // namespace prefs
