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
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/cpp/pref_service_main.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
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
        worker_pool_(std::move(worker_pool)) {}

 protected:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<service_manager::mojom::ServiceFactory>(this);
    return true;
  }

  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == prefs::mojom::kPrefStoreServiceName) {
      pref_service_context_.reset(new service_manager::ServiceContext(
          CreatePrefService(std::set<PrefValueStore::PrefStoreType>(),
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
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> pref_service_context_;
};

constexpr int kInitialValue = 1;
constexpr int kUpdatedValue = 2;
constexpr char kKey[] = "some_key";

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
    connector()->BindInterface(mojom::kPrefStoreServiceName, &control);
    auto config = mojom::PersistentPrefStoreConfiguration::New();
    config->set_simple_configuration(
        mojom::SimplePersistentPrefStoreConfiguration::New(
            profile_dir_.GetPath().AppendASCII("Preferences")));
    control->Init(std::move(config));
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
    ConnectToPrefService(connector(), pref_registry,
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

  DISALLOW_COPY_AND_ASSIGN(PrefServiceFactoryTest);
};

// Check that a single client can set and read back values.
TEST_F(PrefServiceFactoryTest, Basic) {
  auto pref_service = Create();

  // TODO(tibell): Once we have a default store check the value prior to
  // setting.
  pref_service->SetInteger(kKey, kUpdatedValue);
  EXPECT_EQ(kUpdatedValue, pref_service->GetInteger(kKey));
}

// Check that updates in one client eventually propagates to the other.
TEST_F(PrefServiceFactoryTest, MultipleClients) {
  auto pref_service = Create();
  auto pref_service2 = Create();

  // TODO(tibell): Once we have a default store check the value prior to
  // setting.
  pref_service->SetInteger(kKey, kUpdatedValue);
  WaitForPrefChange(pref_service2.get(), kKey);
  EXPECT_EQ(kUpdatedValue, pref_service2->GetInteger(kKey));
}

}  // namespace
}  // namespace prefs
