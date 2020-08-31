// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {
namespace multidevice_setup {

namespace {

bool IsAllowedByPolicy(content::BrowserContext* context) {
  return multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
      Profile::FromBrowserContext(context)->GetPrefs());
}

// Class that wraps MultiDeviceSetupClient in a KeyedService.
class MultiDeviceSetupClientHolder : public KeyedService {
 public:
  explicit MultiDeviceSetupClientHolder(content::BrowserContext* context)
      : profile_(Profile::FromBrowserContext(context)) {
    mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup;
    auto receiver = remote_setup.InitWithNewPipeAndPassReceiver();
    multidevice_setup_client_ =
        MultiDeviceSetupClientImpl::Factory::Create(std::move(remote_setup));

    // NOTE: We bind the receiver asynchronously, because we can't synchronously
    // depend on MultiDeviceSetupServiceFactory at construction time. This is
    // due to a circular dependency among the AndroidSmsServiceFactory,
    // MultiDeviceSetupServiceFactory, and MultiDeviceSetupClientFactory
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MultiDeviceSetupClientHolder::BindService,
                       weak_factory_.GetWeakPtr(), std::move(receiver)));
  }

  MultiDeviceSetupClient* multidevice_setup_client() {
    return multidevice_setup_client_.get();
  }

 private:
  void BindService(mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver) {
    MultiDeviceSetupServiceFactory::GetForProfile(profile_)
        ->BindMultiDeviceSetup(std::move(receiver));
  }

  // KeyedService:
  void Shutdown() override {
    multidevice_setup_client_.reset();
    weak_factory_.InvalidateWeakPtrs();
  }

  Profile* const profile_;
  std::unique_ptr<MultiDeviceSetupClient> multidevice_setup_client_;
  base::WeakPtrFactory<MultiDeviceSetupClientHolder> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupClientHolder);
};

}  // namespace

MultiDeviceSetupClientFactory::MultiDeviceSetupClientFactory()
    : BrowserContextKeyedServiceFactory(
          "MultiDeviceSetupClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
}

MultiDeviceSetupClientFactory::~MultiDeviceSetupClientFactory() = default;

// static
MultiDeviceSetupClient* MultiDeviceSetupClientFactory::GetForProfile(
    Profile* profile) {
  if (!profile)
    return nullptr;

  MultiDeviceSetupClientHolder* holder =
      static_cast<MultiDeviceSetupClientHolder*>(
          MultiDeviceSetupClientFactory::GetInstance()
              ->GetServiceForBrowserContext(profile, true));

  return holder ? holder->multidevice_setup_client() : nullptr;
}

// static
MultiDeviceSetupClientFactory* MultiDeviceSetupClientFactory::GetInstance() {
  return base::Singleton<MultiDeviceSetupClientFactory>::get();
}

KeyedService* MultiDeviceSetupClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (IsAllowedByPolicy(context))
    return new MultiDeviceSetupClientHolder(context);

  return nullptr;
}

bool MultiDeviceSetupClientFactory::ServiceIsNULLWhileTesting() const {
  return service_is_null_while_testing_;
}

}  // namespace multidevice_setup
}  // namespace chromeos
