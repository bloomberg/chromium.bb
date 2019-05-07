// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_manager.h"

#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/service_instance.h"

#if !defined(OS_IOS)
#include "services/service_manager/service_process_launcher.h"
#endif

namespace service_manager {

namespace {

const char kCapability_ServiceManager[] = "service_manager:service_manager";

#if defined(OS_WIN)
const char kServiceExecutableExtension[] = ".service.exe";
#else
const char kServiceExecutableExtension[] = ".service";
#endif

base::ProcessId GetCurrentPid() {
#if defined(OS_IOS)
  // iOS does not support base::Process.
  return 0;
#else
  return base::Process::Current().Pid();
#endif
}

const Identity& GetServiceManagerInstanceIdentity() {
  static base::NoDestructor<Identity> id{service_manager::mojom::kServiceName,
                                         kSystemInstanceGroup, base::Token{},
                                         base::Token::CreateRandom()};
  return *id;
}

}  // namespace

ServiceManager::ServiceManager(std::unique_ptr<ServiceProcessLauncherFactory>
                                   service_process_launcher_factory,
                               const std::vector<Manifest>& manifests)
    : catalog_(manifests),
      service_process_launcher_factory_(
          std::move(service_process_launcher_factory)) {
  Manifest service_manager_manifest =
      ManifestBuilder()
          .WithOptions(ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               Manifest::InstanceSharingPolicy::kSingleton)
                           .Build())
          .ExposeCapability(kCapability_ServiceManager,
                            Manifest::InterfaceList<mojom::ServiceManager>())
          .WithInterfacesBindableOnAnyService(
              service_manager::Manifest::InterfaceList<mojom::ServiceFactory>())
          .Build();
  service_manager_instance_ = CreateServiceInstance(
      GetServiceManagerInstanceIdentity(), service_manager_manifest);
  service_manager_instance_->set_pid(GetCurrentPid());

  mojo::PendingRemote<mojom::Service> remote;
  service_binding_.Bind(remote.InitWithNewPipeAndPassReceiver());
  service_manager_instance_->StartWithRemote(std::move(remote),
                                             mojo::NullReceiver());
}

ServiceManager::~ServiceManager() {
  // Stop all of the instances before destroying any of them. This ensures that
  // all Services will receive connection errors and avoids possible deadlock in
  // the case where one ServiceInstance's destructor blocks waiting for its
  // Runner to quit, while that Runner's corresponding Service blocks its
  // shutdown on a distinct Service receiving a connection error.
  //
  // Also ensure we tear down the ServiceManager instance last. This is to avoid
  // hitting bindings DCHECKs, since the ServiceManager or Catalog may at any
  // given time own in-flight responders for Instances' Connector requests.
  for (auto& instance : instances_) {
    if (instance.get() != service_manager_instance_)
      instance->Stop();
  }
  service_manager_instance_->Stop();
  instances_.clear();
}

void ServiceManager::SetInstanceQuitCallback(
    base::Callback<void(const Identity&)> callback) {
  instance_quit_callback_ = std::move(callback);
}

void ServiceManager::Connect(
    const ServiceFilter& partial_target_filter,
    const Identity& source_identity,
    const base::Optional<std::string>& interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe,
    mojo::PendingRemote<mojom::Service> service_remote,
    mojo::PendingReceiver<mojom::PIDReceiver> pid_receiver,
    mojom::BindInterfacePriority priority,
    mojom::Connector::BindInterfaceCallback callback) {
  TRACE_EVENT_INSTANT1("service_manager", "ServiceManager::Connect",
                       TRACE_EVENT_SCOPE_THREAD, "original_name",
                       partial_target_filter.service_name());
  DCHECK(source_identity.IsValid());

  ServiceFilter target_filter = partial_target_filter;

  // If the target filter does not specify an instance group, we assume the
  // source's own.
  if (!target_filter.instance_group())
    target_filter.set_instance_group(source_identity.instance_group());

  // If the target filter does not specify an instance ID, we assume zero.
  if (!target_filter.instance_id())
    target_filter.set_instance_id(base::Token());

  if (!service_remote) {
    // Connect to an existing matching instance, if possible.
    ServiceInstance* instance = instance_registry_.FindMatching(target_filter);
    if (instance) {
      if (interface_name) {
        DCHECK(receiving_pipe.is_valid());
        instance->AuthorizeAndForwardConnectionRequestFromOtherService(
            source_identity, interface_name.value(), std::move(receiving_pipe),
            priority, std::move(callback));
      } else {
        // This is a StartService request and the instance is already running.
        // Make sure the response identity is properly resolved.
        std::move(callback).Run(mojom::ConnectResult::SUCCEEDED,
                                instance->identity());
      }
      return;
    }

    // If there was no existing instance but the request specified a specific
    // globally unique ID for the target, ignore the request. That instance is
    // obviously no longer running.
    if (target_filter.globally_unique_id()) {
      std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED,
                              base::nullopt);
      return;
    }
  }

  // Beyond this point, in order to fulfill the connection request we need to
  // start a new instance of the target service.

  const service_manager::Manifest* manifest =
      catalog_.GetManifest(target_filter.service_name());
  if (!manifest) {
    LOG(ERROR) << "Failed to resolve service name: "
               << target_filter.service_name();
    std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT,
                            base::nullopt);
    return;
  }

  // Services that use |kSharedAcrossGroups| sharing policy are allowed to
  // handle connection requests from instances in any instance group. They also
  // run with a synthetic group ID generated below and the group provided by
  // |target_filter| is ignored.
  Identity new_instance_identity;
  if (service_remote) {
    // This is a service instance registration, so we should use the exact
    // Identity given by the caller.
    if (!target_filter.globally_unique_id() ||
        target_filter.globally_unique_id()->is_zero()) {
      std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT,
                              base::nullopt);
      return;
    }
    new_instance_identity = Identity(
        target_filter.service_name(), *target_filter.instance_group(),
        *target_filter.instance_id(), *target_filter.globally_unique_id());
  } else if (manifest->options.instance_sharing_policy ==
             Manifest::InstanceSharingPolicy::kSingleton) {
    // For singleton instances, we generate a random group ID along with the
    // random GUID.
    new_instance_identity =
        Identity(target_filter.service_name(), base::Token::CreateRandom(),
                 base::Token{}, base::Token::CreateRandom());
  } else if (manifest->options.instance_sharing_policy ==
             Manifest::InstanceSharingPolicy::kSharedAcrossGroups) {
    // For services shared across instance groups, we respect the target
    // instance ID but still generate a random group ID along with the random
    // GUID.
    new_instance_identity =
        Identity(target_filter.service_name(), base::Token::CreateRandom(),
                 *target_filter.instance_id(), base::Token::CreateRandom());
  } else {
    DCHECK_EQ(manifest->options.instance_sharing_policy,
              Manifest::InstanceSharingPolicy::kNoSharing);
    new_instance_identity =
        Identity(target_filter.service_name(), *target_filter.instance_group(),
                 *target_filter.instance_id(), base::Token::CreateRandom());
  }

  ServiceInstance* instance =
      CreateServiceInstance(new_instance_identity, *manifest);

  if (service_remote) {
    // If this is a service registration via |RegisterService()| or
    // |Connector.RegisterServiceInstance()|, we'll have a valid
    // |service_remote| already and that's enough to be done.
    instance->StartWithRemote(std::move(service_remote),
                              std::move(pid_receiver));
    std::move(callback).Run(mojom::ConnectResult::SUCCEEDED,
                            instance->identity());
    return;
  }

  const service_manager::Manifest* parent_manifest =
      catalog_.GetParentManifest(manifest->service_name);
  if (parent_manifest) {
    // This service is provided by another service via a ServiceFactory.
    //
    // We normally ignore the target instance group and generate a unique
    // instance group identifier when starting shared instances, but when those
    // instances need to be started by a service factory, it's conceivable that
    // the factory itself may be part of the originally targeted instance group.
    // In that case we use the originally targeted instance group to identify
    // the factory service.
    //
    // TODO(https://crbug.com/904240): This is super weird and hard to
    // rationalize. Maybe it's the wrong thing to do.
    instance->set_identity(
        Identity(new_instance_identity.name(), *target_filter.instance_group(),
                 new_instance_identity.instance_id(),
                 new_instance_identity.globally_unique_id()));

    auto factory_filter = ServiceFilter::ByNameWithIdInGroup(
        parent_manifest->service_name, *target_filter.instance_id(),
        *target_filter.instance_group());

    mojom::PIDReceiverPtr pid_receiver;
    auto pid_receiver_request = mojo::MakeRequest(&pid_receiver);

    mojo::PendingRemote<mojom::Service> remote;
    CreateServiceWithFactory(factory_filter, target_filter.service_name(),
                             remote.InitWithNewPipeAndPassReceiver(),
                             std::move(pid_receiver));
    instance->StartWithRemote(std::move(remote),
                              std::move(pid_receiver_request));
  } else {
    base::FilePath service_exe_root;
    CHECK(base::PathService::Get(base::DIR_ASSETS, &service_exe_root));
    if (!instance->StartWithExecutablePath(
            service_exe_root.AppendASCII(manifest->service_name +
                                         kServiceExecutableExtension),
            UtilitySandboxTypeFromString(manifest->options.sandbox_type))) {
      DestroyInstance(instance);
      std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT,
                              base::nullopt);
      return;
    }
  }

  if (interface_name) {
    DCHECK(receiving_pipe);
    instance->AuthorizeAndForwardConnectionRequestFromOtherService(
        source_identity, interface_name.value(), std::move(receiving_pipe),
        priority, std::move(callback));
    return;
  }

  std::move(callback).Run(mojom::ConnectResult::SUCCEEDED,
                          instance->identity());
}

void ServiceManager::Connect(const ServiceFilter& partial_target_filter,
                             const Identity& source_identity,
                             const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle receiving_pipe,
                             mojom::BindInterfacePriority priority,
                             mojom::Connector::BindInterfaceCallback callback) {
  Connect(partial_target_filter, source_identity, interface_name,
          std::move(receiving_pipe), mojo::NullRemote(), mojo::NullReceiver(),
          priority, std::move(callback));
}

void ServiceManager::Connect(const ServiceFilter& partial_target_filter,
                             const Identity& source_identity,
                             mojom::Connector::BindInterfaceCallback callback) {
  Connect(partial_target_filter, source_identity,
          base::nullopt /* interface_name */, mojo::ScopedMessagePipeHandle(),
          mojo::NullRemote(), mojo::NullReceiver(),
          mojom::BindInterfacePriority::kBestEffort, std::move(callback));
}

void ServiceManager::Connect(
    const ServiceFilter& partial_target_filter,
    const Identity& source_identity,
    mojo::PendingRemote<mojom::Service> service_remote,
    mojo::PendingReceiver<mojom::PIDReceiver> pid_receiver,
    mojom::Connector::BindInterfaceCallback callback) {
  Connect(partial_target_filter, source_identity,
          base::nullopt /* interface_name */, mojo::ScopedMessagePipeHandle(),
          std::move(service_remote), std::move(pid_receiver),
          mojom::BindInterfacePriority::kImportant, std::move(callback));
}

void ServiceManager::StartService(const std::string& service_name) {
  Connect(ServiceFilter::ByNameInGroup(service_name, kSystemInstanceGroup),
          GetServiceManagerInstanceIdentity(), base::DoNothing());
}

bool ServiceManager::QueryCatalog(const std::string& service_name,
                                  const base::Token& instance_group,
                                  std::string* sandbox_type) {
  const Manifest* manifest = catalog_.GetManifest(service_name);
  if (!manifest)
    return false;
  *sandbox_type = manifest->options.sandbox_type;
  return true;
}

void ServiceManager::RegisterService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  if (!pid_receiver_request.is_pending()) {
    mojo::Remote<mojom::PIDReceiver> pid_receiver;
    pid_receiver_request = pid_receiver.BindNewPipeAndPassReceiver();
    pid_receiver->SetPID(GetCurrentPid());
  }

  DCHECK(identity.IsValid());
  Connect(ServiceFilter::ForExactIdentity(identity), identity,
          service.PassInterface(), std::move(pid_receiver_request),
          base::DoNothing());
}

void ServiceManager::MakeInstanceUnreachable(ServiceInstance* instance) {
  instance_registry_.Unregister(instance);
}

void ServiceManager::DestroyInstance(ServiceInstance* instance) {
  // We never clean up the ServiceManager's own instance.
  if (instance == service_manager_instance_)
    return;

  MakeInstanceUnreachable(instance);
  auto it = instances_.find(instance);
  DCHECK(it != instances_.end());

  // Deletes |instance|.
  instances_.erase(it);
}

void ServiceManager::OnInstanceStopped(const Identity& identity) {
  listeners_.ForAllPtrs([&identity](mojom::ServiceManagerListener* listener) {
    listener->OnServiceStopped(identity);
  });
  if (!instance_quit_callback_.is_null())
    instance_quit_callback_.Run(identity);
}

ServiceInstance* ServiceManager::GetExistingInstance(
    const Identity& identity) const {
  return instance_registry_.FindMatching(
      ServiceFilter::ForExactIdentity(identity));
}

void ServiceManager::NotifyServiceCreated(ServiceInstance* instance) {
  mojom::RunningServiceInfoPtr info = instance->CreateRunningServiceInfo();
  listeners_.ForAllPtrs([&info](mojom::ServiceManagerListener* listener) {
    listener->OnServiceCreated(info.Clone());
  });
}

void ServiceManager::NotifyServiceStarted(const Identity& identity,
                                          base::ProcessId pid) {
  listeners_.ForAllPtrs(
      [&identity, pid](mojom::ServiceManagerListener* listener) {
        listener->OnServiceStarted(identity, pid);
      });
}

void ServiceManager::NotifyServiceFailedToStart(const Identity& identity) {
  listeners_.ForAllPtrs([&identity](mojom::ServiceManagerListener* listener) {
    listener->OnServiceFailedToStart(identity);
  });
}

void ServiceManager::NotifyServicePIDReceived(const Identity& identity,
                                              base::ProcessId pid) {
  listeners_.ForAllPtrs(
      [&identity, pid](mojom::ServiceManagerListener* listener) {
        listener->OnServicePIDReceived(identity, pid);
      });
}

ServiceInstance* ServiceManager::CreateServiceInstance(
    const Identity& identity,
    const Manifest& manifest) {
  DCHECK(identity.IsValid());

  auto instance = std::make_unique<ServiceInstance>(this, identity, manifest);
  auto* raw_instance = instance.get();

  instances_.insert(std::move(instance));

  // NOTE: |instance| has been passed elsewhere. Use |raw_instance| from this
  // point forward. It's safe for the extent of this method.

  instance_registry_.Register(raw_instance);

  return raw_instance;
}

void ServiceManager::AddListener(mojom::ServiceManagerListenerPtr listener) {
  std::vector<mojom::RunningServiceInfoPtr> infos;
  for (auto& instance : instances_)
    infos.push_back(instance->CreateRunningServiceInfo());

  listener->OnInit(std::move(infos));
  listeners_.AddPtr(std::move(listener));
}

void ServiceManager::CreateServiceWithFactory(
    const ServiceFilter& service_factory_filter,
    const std::string& name,
    mojom::ServiceRequest request,
    mojom::PIDReceiverPtr pid_receiver) {
  mojom::ServiceFactory* factory = GetServiceFactory(service_factory_filter);
  factory->CreateService(std::move(request), name, std::move(pid_receiver));
}

mojom::ServiceFactory* ServiceManager::GetServiceFactory(
    const ServiceFilter& filter) {
  auto it = service_factories_.find(filter);
  if (it != service_factories_.end())
    return it->second.get();

  mojom::ServiceFactoryPtr factory;
  Connect(filter, GetServiceManagerInstanceIdentity(),
          mojom::ServiceFactory::Name_,
          mojo::MakeRequest(&factory).PassMessagePipe(),
          mojom::BindInterfacePriority::kImportant, base::DoNothing());

  mojom::ServiceFactory* factory_interface = factory.get();
  factory.set_connection_error_handler(base::BindOnce(
      &ServiceManager::OnServiceFactoryLost, base::Unretained(this), filter));
  service_factories_[filter] = std::move(factory);
  return factory_interface;
}

void ServiceManager::OnServiceFactoryLost(const ServiceFilter& which) {
  // Remove the mapping.
  auto it = service_factories_.find(which);
  DCHECK(it != service_factories_.end());
  service_factories_.erase(it);
}

void ServiceManager::OnBindInterface(
    const BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe) {
  ServiceInstance* instance = GetExistingInstance(source_info.identity);
  DCHECK(instance);
  if (interface_name == mojom::ServiceManager::Name_) {
    instance->BindServiceManagerReceiver(
        mojo::PendingReceiver<mojom::ServiceManager>(
            std::move(receiving_pipe)));
  }
}

}  // namespace service_manager
