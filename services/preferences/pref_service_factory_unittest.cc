// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_service_factory.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/value_map_pref_store.h"
#include "components/prefs/writeable_pref_store.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/cpp/pref_service_main.h"
#include "services/preferences/public/cpp/pref_store_impl.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"

namespace prefs {
namespace {

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory,
                          public service_manager::InterfaceFactory<
                              service_manager::mojom::ServiceFactory> {
 public:
  ServiceTestClient(service_manager::test::ServiceTest* test,
                    scoped_refptr<base::SequencedWorkerPool> worker_pool)
      : service_manager::test::ServiceTestClient(test),
        worker_pool_(std::move(worker_pool)) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(this);
  }

 protected:
  void OnBindInterface(const service_manager::ServiceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(source_info.identity, interface_name,
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

  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ServiceFactoryRequest request) override {
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

class PrefServiceFactoryTest : public base::MessageLoop::DestructionObserver,
                               public service_manager::test::ServiceTest {
 public:
  PrefServiceFactoryTest() : ServiceTest("prefs_unittests", false) {}

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
    return base::MakeUnique<ServiceTestClient>(this,
                                               worker_pool_owner_->pool());
  }

  std::unique_ptr<base::MessageLoop> CreateMessageLoop() override {
    auto loop = ServiceTest::CreateMessageLoop();
    worker_pool_owner_ = base::MakeUnique<base::SequencedWorkerPoolOwner>(
        2, "PrefServiceFactoryTest");
    loop->AddDestructionObserver(this);
    return loop;
  }

  // base::MessageLoop::DestructionObserver
  void WillDestroyCurrentMessageLoop() override { worker_pool_owner_.reset(); }

  // Create a fully initialized PrefService synchronously.
  std::unique_ptr<PrefService> Create() {
    std::unique_ptr<PrefService> pref_service;
    base::RunLoop run_loop;
    auto pref_registry = make_scoped_refptr(new PrefRegistrySimple());
    pref_registry->RegisterIntegerPref(kKey, kInitialValue);
    pref_registry->RegisterIntegerPref(kOtherKey, kInitialValue);
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
  std::unique_ptr<base::SequencedWorkerPoolOwner> worker_pool_owner_;
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

}  // namespace
}  // namespace prefs
