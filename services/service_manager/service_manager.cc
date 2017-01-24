// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_manager.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/service_manager/connect_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/public/interfaces/service_control.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"
#include "services/tracing/public/interfaces/constants.mojom.h"

namespace service_manager {

namespace {

const char kCapability_UserID[] = "service_manager:user_id";
const char kCapability_ClientProcess[] = "service_manager:client_process";
const char kCapability_InstanceName[] = "service_manager:instance_name";
const char kCapability_AllUsers[] = "service_manager:all_users";
const char kCapability_InstancePerChild[] =
    "service_manager:instance_per_child";
const char kCapability_ServiceManager[] = "service_manager:service_manager";

bool Succeeded(mojom::ConnectResult result) {
  return result == mojom::ConnectResult::SUCCEEDED;
}

bool RunConnectCallback(ConnectParams* params,
                        mojom::ConnectResult result,
                        const std::string& user_id) {
  if (!params->connect_callback().is_null()) {
    params->connect_callback().Run(result, user_id);
    return true;
  }
  return false;
}

void RunBindInterfaceCallback(ConnectParams* params,
                              mojom::ConnectResult result,
                              const std::string& user_id) {
  if (!params->bind_interface_callback().is_null())
    params->bind_interface_callback().Run(result, user_id);
}

void RunCallback(ConnectParams* params,
                 mojom::ConnectResult result,
                 const std::string& user_id) {
  if (!RunConnectCallback(params, result, user_id))
    RunBindInterfaceCallback(params, result, user_id);
}

}  // namespace

Identity CreateServiceManagerIdentity() {
  return Identity(service_manager::mojom::kServiceName, mojom::kRootUserID);
}

Identity CreateCatalogIdentity() {
  return Identity(catalog::mojom::kServiceName, mojom::kRootUserID);
}

InterfaceProviderSpec GetPermissiveInterfaceProviderSpec() {
  InterfaceProviderSpec spec;
  InterfaceSet interfaces;
  interfaces.insert("*");
  spec.requires["*"] = interfaces;
  return spec;
}

bool HasCapability(const InterfaceProviderSpec& spec,
                   const std::string& capability) {
  auto it = spec.requires.find(service_manager::mojom::kServiceName);
  if (it == spec.requires.end())
    return false;
  return it->second.find(capability) != it->second.end();
}

// Encapsulates a connection to an instance of a service, tracked by the
// Service Manager.
class ServiceManager::Instance
    : public mojom::Connector,
      public mojom::PIDReceiver,
      public Service,
      public InterfaceFactory<mojom::ServiceManager>,
      public mojom::ServiceManager,
      public mojom::ServiceControl {
 public:
  Instance(service_manager::ServiceManager* service_manager,
           const Identity& identity,
           const InterfaceProviderSpecMap& interface_provider_specs)
      : service_manager_(service_manager),
        id_(GenerateUniqueID()),
        identity_(identity),
        interface_provider_specs_(interface_provider_specs),
        allow_any_application_(GetConnectionSpec().requires.count("*") == 1),
        pid_receiver_binding_(this),
        control_binding_(this),
        state_(State::IDLE),
        weak_factory_(this) {
    if (identity_.name() == service_manager::mojom::kServiceName ||
        identity_.name() == catalog::mojom::kServiceName) {
      pid_ = base::Process::Current().Pid();
    }
    DCHECK_NE(mojom::kInvalidInstanceID, id_);
  }

  void Stop() {
    // Shutdown all bindings. This way the process should see the pipes closed
    // and exit, as well as waking up any potential
    // sync/WaitForIncomingResponse().
    service_.reset();
    if (pid_receiver_binding_.is_bound())
      pid_receiver_binding_.Close();
    connectors_.CloseAllBindings();
    service_manager_bindings_.CloseAllBindings();
    service_manager_->OnInstanceUnreachable(this);

    if (state_ == State::STARTING) {
      service_manager_->NotifyServiceFailedToStart(identity_);
    } else {
      // Notify the ServiceManager that this Instance is really going away.
      service_manager_->OnInstanceStopped(identity_);
    }
  }

  ~Instance() override {
    Stop();
  }

  bool CallOnConnect(std::unique_ptr<ConnectParams>* in_params) {
    if (!service_.is_bound()) {
      RunConnectCallback(in_params->get(), mojom::ConnectResult::ACCESS_DENIED,
                         identity_.user_id());
      return false;
    }

    std::unique_ptr<ConnectParams> params(std::move(*in_params));
    RunConnectCallback(params.get(), mojom::ConnectResult::SUCCEEDED,
                       identity_.user_id());

    InterfaceProviderSpecMap specs;
    Instance* source =
        service_manager_->GetExistingInstance(params->source());
    if (source)
      specs = source->interface_provider_specs_;

    pending_service_connections_++;
    service_->OnConnect(ServiceInfo(params->source(), specs),
                        params->TakeRemoteInterfaces(),
                        base::Bind(&Instance::OnConnectComplete,
                                   base::Unretained(this)));
    return true;
  }

  bool CallOnBindInterface(std::unique_ptr<ConnectParams>* in_params) {
    if (!service_.is_bound()) {
      RunBindInterfaceCallback(in_params->get(),
                               mojom::ConnectResult::ACCESS_DENIED,
                               identity_.user_id());
      return false;
    }

    std::unique_ptr<ConnectParams> params(std::move(*in_params));
    InterfaceProviderSpecMap source_specs;
    InterfaceProviderSpec source_connection_spec;
    Instance* source =
        service_manager_->GetExistingInstance(params->source());
    if (source) {
      source_specs = source->interface_provider_specs_;
      source_connection_spec = source->GetConnectionSpec();
    }

    InterfaceSet exposed = GetInterfacesToExpose(source_connection_spec,
                                                 identity_,
                                                 GetConnectionSpec());
    bool allowed = (exposed.size() == 1 && exposed.count("*") == 1) ||
        exposed.count(params->interface_name()) > 0;
    if (!allowed) {
      std::stringstream ss;
      ss << "Connection InterfaceProviderSpec prevented service: "
         << params->source().name() << " from binding interface: "
         << params->interface_name() << " exposed by: " << identity_.name();
      LOG(ERROR) << ss.str();
      params->bind_interface_callback().Run(mojom::ConnectResult::ACCESS_DENIED,
                                            identity_.user_id());
      return false;
    }

    params->bind_interface_callback().Run(mojom::ConnectResult::SUCCEEDED,
                                          identity_.user_id());

    pending_service_connections_++;
    service_->OnBindInterface(
        ServiceInfo(params->source(), source_specs),
        params->interface_name(),
        params->TakeInterfaceRequestPipe(),
        base::Bind(&Instance::OnConnectComplete, base::Unretained(this)));
    return true;
  }

  void OnConnectComplete() {
    DCHECK_GT(pending_service_connections_, 0);
    pending_service_connections_--;
  }

  void StartWithService(mojom::ServicePtr service) {
    CHECK(!service_);
    state_ = State::STARTING;
    service_ = std::move(service);
    service_.set_connection_error_handler(
        base::Bind(&Instance::OnServiceLost, base::Unretained(this),
                   service_manager_->GetWeakPtr()));
    service_->OnStart(ServiceInfo(identity_, interface_provider_specs_),
                      base::Bind(&Instance::OnStartComplete,
                                 base::Unretained(this)));
  }

  bool StartWithFilePath(const base::FilePath& path) {
    DCHECK(!service_);
    DCHECK(!path.empty());
    runner_ = service_manager_->service_process_launcher_factory_->Create(path);
    if (!runner_)
      return false;
    bool start_sandboxed = false;
    mojom::ServicePtr service = runner_->Start(
        identity_, start_sandboxed,
        base::Bind(&Instance::PIDAvailable, weak_factory_.GetWeakPtr()));
    StartWithService(std::move(service));
    return true;
  }

  void BindPIDReceiver(mojom::PIDReceiverRequest request) {
    pid_receiver_binding_.Bind(std::move(request));
  }

  mojom::RunningServiceInfoPtr CreateRunningServiceInfo() const {
    mojom::RunningServiceInfoPtr info(mojom::RunningServiceInfo::New());
    info->id = id_;
    info->identity = identity_;
    info->pid = pid_;
    return info;
  }

  const InterfaceProviderSpec& GetConnectionSpec() const {
    auto it = interface_provider_specs_.find(
        mojom::kServiceManager_ConnectorSpec);
    return it != interface_provider_specs_.end() ? it->second : empty_spec_;
  }
  const Identity& identity() const { return identity_; }
  void set_identity(const Identity& identity) { identity_ = identity; }
  uint32_t id() const { return id_; }

  // Service:
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    Instance* source =
        service_manager_->GetExistingInstance(remote_info.identity);
    DCHECK(source);
    if (HasCapability(source->GetConnectionSpec(),
                      kCapability_ServiceManager)) {
      registry->AddInterface<mojom::ServiceManager>(this);
      return true;
    }
    return false;
  }

 private:
   enum class State {
    // The service was not started yet.
    IDLE,

    // The service was started but the service manager hasn't received the
    // initial response from it yet.
    STARTING,

    // The service was started successfully.
    STARTED
  };

  // mojom::Connector implementation:
  void StartService(
      const Identity& in_target,
      mojo::ScopedMessagePipeHandle service_handle,
      mojom::PIDReceiverRequest pid_receiver_request) override {
    Identity target = in_target;
    mojom::ConnectResult result =
        ValidateConnectParams(&target, nullptr, nullptr);
    if (!Succeeded(result))
      return;

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);

    mojom::ServicePtr service;
    service.Bind(mojom::ServicePtrInfo(std::move(service_handle), 0));
    params->set_client_process_info(std::move(service),
                                    std::move(pid_receiver_request));
    service_manager_->Connect(std::move(params), weak_factory_.GetWeakPtr());
  }

  void Connect(const service_manager::Identity& in_target,
               mojom::InterfaceProviderRequest remote_interfaces,
               const ConnectCallback& callback) override {
    Identity target = in_target;
    mojom::ConnectResult result =
        ValidateConnectParams(&target, nullptr, nullptr);
    if (!Succeeded(result)) {
      callback.Run(result, mojom::kInheritUserID);
      return;
    }

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);
    params->set_remote_interfaces(std::move(remote_interfaces));
    params->set_connect_callback(callback);
    service_manager_->Connect(std::move(params), weak_factory_.GetWeakPtr());
  }

  void BindInterface(const service_manager::Identity& in_target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     const BindInterfaceCallback& callback) override {
    Identity target = in_target;
    mojom::ConnectResult result =
        ValidateConnectParams(&target, nullptr, nullptr);
    if (!Succeeded(result)) {
      callback.Run(result, mojom::kInheritUserID);
      return;
    }

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);
    params->set_interface_request_info(interface_name,
                                       std::move(interface_pipe));
    params->set_bind_interface_callback(callback);
    service_manager_->Connect(std::move(params), weak_factory_.GetWeakPtr());
  }

  void Clone(mojom::ConnectorRequest request) override {
    connectors_.AddBinding(this, std::move(request));
  }

  // mojom::PIDReceiver:
  void SetPID(uint32_t pid) override {
    PIDAvailable(pid);
  }

  // InterfaceFactory<mojom::ServiceManager>:
  void Create(const Identity& remote_identity,
              mojom::ServiceManagerRequest request) override {
    service_manager_bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ServiceManager implementation:
  void AddListener(mojom::ServiceManagerListenerPtr listener) override {
    // TODO(beng): this should only track the instances matching this user, and
    // root.
    service_manager_->AddListener(std::move(listener));
  }

  mojom::ConnectResult ValidateConnectParams(
      Identity* target,
      mojom::ServicePtr* service,
      mojom::PIDReceiverRequest* pid_receiver_request) {
    if (target->user_id() == mojom::kInheritUserID)
      target->set_user_id(identity_.user_id());

    mojom::ConnectResult result = ValidateIdentity(*target);
    if (!Succeeded(result))
      return result;

    result = ValidateClientProcessInfo(service, pid_receiver_request, *target);
    if (!Succeeded(result))
      return result;
    return ValidateConnectionSpec(*target);
  }

  mojom::ConnectResult ValidateIdentity(const Identity& identity) {
    if (identity.name().empty()) {
      LOG(ERROR) << "Error: empty service name.";
      return mojom::ConnectResult::INVALID_ARGUMENT;
    }
    if (!base::IsValidGUID(identity.user_id())) {
      LOG(ERROR) << "Error: invalid user_id: " << identity.user_id();
      return mojom::ConnectResult::INVALID_ARGUMENT;
    }
    return mojom::ConnectResult::SUCCEEDED;
  }

  mojom::ConnectResult ValidateClientProcessInfo(
      mojom::ServicePtr* service,
      mojom::PIDReceiverRequest* pid_receiver_request,
      const Identity& target) {
    if (service && pid_receiver_request &&
        (service->is_bound() || pid_receiver_request->is_pending())) {
      if (!HasCapability(GetConnectionSpec(), kCapability_ClientProcess)) {
        LOG(ERROR) << "Instance: " << identity_.name() << " attempting "
                   << "to register an instance for a process it created for "
                   << "target: " << target.name() << " without the "
                   << "service_manager{client_process} capability "
                   << "class.";
        return mojom::ConnectResult::ACCESS_DENIED;
      }

      if (!service->is_bound() || !pid_receiver_request->is_pending()) {
        LOG(ERROR) << "Must supply both service AND "
                   << "pid_receiver_request when sending client process info";
        return mojom::ConnectResult::INVALID_ARGUMENT;
      }
      if (service_manager_->GetExistingInstance(target)) {
        LOG(ERROR) << "Cannot client process matching existing identity:"
                   << "Name: " << target.name() << " User: "
                   << target.user_id() << " Instance: " << target.instance();
        return mojom::ConnectResult::INVALID_ARGUMENT;
      }
    }
    return mojom::ConnectResult::SUCCEEDED;
  }

  mojom::ConnectResult ValidateConnectionSpec(const Identity& target) {
    InterfaceProviderSpec connection_spec = GetConnectionSpec();
    // TODO(beng): Need to do the following additional policy validation of
    // whether this instance is allowed to connect using:
    // - non-null client process info.
    if (target.user_id() != identity_.user_id() &&
        target.user_id() != mojom::kRootUserID &&
        !HasCapability(connection_spec, kCapability_UserID)) {
      LOG(ERROR) << "Instance: " << identity_.name()
                 << " running as: " << identity_.user_id()
                 << " attempting to connect to: " << target.name()
                 << " as: " << target.user_id() << " without "
                 << " the service:service_manager{user_id} capability.";
      return mojom::ConnectResult::ACCESS_DENIED;
    }
    if (!target.instance().empty() &&
        target.instance() != target.name() &&
        !HasCapability(connection_spec, kCapability_InstanceName)) {
      LOG(ERROR) << "Instance: " << identity_.name() << " attempting to "
                 << "connect to " << target.name()
                 << " using Instance name: " << target.instance()
                 << " without the "
                 << "service_manager{instance_name} capability.";
      return mojom::ConnectResult::ACCESS_DENIED;
    }

    if (allow_any_application_ ||
        connection_spec.requires.find(target.name()) !=
            connection_spec.requires.end()) {
      return mojom::ConnectResult::SUCCEEDED;
    }
    LOG(ERROR) << "InterfaceProviderSpec prevented connection from: "
               << identity_.name() << " to: " << target.name();
    return mojom::ConnectResult::ACCESS_DENIED;
  }

  uint32_t GenerateUniqueID() const {
    static uint32_t id = mojom::kInvalidInstanceID;
    ++id;
    CHECK_NE(mojom::kInvalidInstanceID, id);
    return id;
  }

  void PIDAvailable(base::ProcessId pid) {
    if (pid == base::kNullProcessId) {
      service_manager_->OnInstanceError(this);
      return;
    }
    pid_ = pid;
  }

  void OnServiceLost(
      base::WeakPtr<service_manager::ServiceManager> service_manager) {
    service_.reset();
    OnConnectionLost(service_manager);
  }

  void OnConnectionLost(
      base::WeakPtr<service_manager::ServiceManager> service_manager) {
    // Any time a Connector is lost or we lose the Service connection, it
    // may have been the last pipe using this Instance. If so, clean up.
    if (service_manager && !service_) {
      if (connectors_.empty())
        service_manager->OnInstanceError(this);
      else
        service_manager->OnInstanceUnreachable(this);
    }
  }

  void OnStartComplete(mojom::ConnectorRequest connector_request,
                       mojom::ServiceControlAssociatedRequest control_request) {
    state_ = State::STARTED;
    if (connector_request.is_pending()) {
      connectors_.AddBinding(this, std::move(connector_request));
      connectors_.set_connection_error_handler(
          base::Bind(&Instance::OnConnectionLost, base::Unretained(this),
                     service_manager_->GetWeakPtr()));
    }
    if (control_request.is_pending())
      control_binding_.Bind(std::move(control_request));
    service_manager_->NotifyServiceStarted(identity_, pid_);
  }

  // mojom::ServiceControl:
  void RequestQuit() override {
    // If quit is requested, oblige when there are no pending OnConnects.
    if (!pending_service_connections_)
      OnServiceLost(service_manager_->GetWeakPtr());
  }

  void EmptyConnectCallback(mojom::ConnectResult result,
                            const std::string& user_id) {}
  void BindCallbackWrapper(const BindInterfaceCallback& wrapped,
                           mojom::ConnectResult result,
                           const std::string& user_id) {
    wrapped.Run(result, user_id);
  }

  service_manager::ServiceManager* const service_manager_;

  // An id that identifies this instance. Distinct from pid, as a single process
  // may vend multiple application instances, and this object may exist before a
  // process is launched.
  const uint32_t id_;
  Identity identity_;
  const InterfaceProviderSpecMap interface_provider_specs_;
  const InterfaceProviderSpec empty_spec_;
  const bool allow_any_application_;
  std::unique_ptr<ServiceProcessLauncher> runner_;
  mojom::ServicePtr service_;
  mojo::Binding<mojom::PIDReceiver> pid_receiver_binding_;
  mojo::BindingSet<mojom::Connector> connectors_;
  mojo::BindingSet<mojom::ServiceManager> service_manager_bindings_;
  mojo::AssociatedBinding<mojom::ServiceControl> control_binding_;
  base::ProcessId pid_ = base::kNullProcessId;
  State state_;

  // The number of outstanding OnConnect requests which are in flight.
  int pending_service_connections_ = 0;

  base::WeakPtrFactory<Instance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

class ServiceManager::ServiceImpl : public Service {
 public:
  explicit ServiceImpl(ServiceManager* service_manager)
      : service_manager_(service_manager) {}
  ~ServiceImpl() override {}

  // Service:
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    // The only interface ServiceManager exposes is mojom::ServiceManager, and
    // access to this interface is brokered by a policy specific to each caller,
    // managed by the caller's instance. Here we look to see who's calling,
    // and forward to the caller's instance to continue.
    Instance* instance = nullptr;
    for (const auto& entry : service_manager_->identity_to_instance_) {
      if (entry.first == remote_info.identity) {
        instance = entry.second;
        break;
      }
    }

    DCHECK(instance);
    return instance->OnConnect(remote_info, registry);
  }

 private:
  ServiceManager* const service_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

// static
ServiceManager::TestAPI::TestAPI(ServiceManager* service_manager)
    : service_manager_(service_manager) {}
ServiceManager::TestAPI::~TestAPI() {}

bool ServiceManager::TestAPI::HasRunningInstanceForName(
    const std::string& name) const {
  for (const auto& entry : service_manager_->identity_to_instance_) {
    if (entry.first.name() == name)
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManager, public:

ServiceManager::ServiceManager(
    std::unique_ptr<ServiceProcessLauncherFactory>
        service_process_launcher_factory,
    mojom::ServicePtr catalog)
    : service_process_launcher_factory_(
          std::move(service_process_launcher_factory)),
      weak_ptr_factory_(this) {
  mojom::ServicePtr service;
  mojom::ServiceRequest request(&service);

  InterfaceProviderSpec spec;
  spec.provides[kCapability_ServiceManager].insert(
      "service_manager::mojom::ServiceManager");
  spec.requires["*"].insert("service_manager:service_factory");
  spec.requires[catalog::mojom::kServiceName].insert(
      "service_manager:resolver");
  spec.requires[tracing::mojom::kServiceName].insert("app");
  InterfaceProviderSpecMap specs;
  specs[mojom::kServiceManager_ConnectorSpec] = spec;

  service_manager_instance_ =
      CreateInstance(Identity(), CreateServiceManagerIdentity(), specs);
  service_manager_instance_->StartWithService(std::move(service));
  singletons_.insert(service_manager::mojom::kServiceName);
  service_context_.reset(new ServiceContext(
      base::MakeUnique<ServiceImpl>(this), std::move(request)));

  if (catalog)
    InitCatalog(std::move(catalog));
}

ServiceManager::~ServiceManager() {
  // Ensure we tear down the ServiceManager instance last. This is to avoid
  // hitting bindings DCHECKs, since the ServiceManager or Catalog may at any
  // given time own in-flight responders for Instances' Connector requests.
  std::unique_ptr<Instance> service_manager_instance;
  auto iter = instances_.find(service_manager_instance_);
  DCHECK(iter != instances_.end());
  service_manager_instance = std::move(iter->second);

  // Stop all of the instances before destroying any of them. This ensures that
  // all Services will receive connection errors and avoids possible deadlock in
  // the case where one Instance's destructor blocks waiting for its Runner to
  // quit, while that Runner's corresponding Service blocks its shutdown on a
  // distinct Service receiving a connection error.
  for (const auto& instance : instances_)
    instance.first->Stop();
  service_manager_instance->Stop();

  instances_.clear();
}

void ServiceManager::SetServiceOverrides(
    std::unique_ptr<ServiceOverrides> overrides) {
  service_overrides_ = std::move(overrides);
}

void ServiceManager::SetInstanceQuitCallback(
    base::Callback<void(const Identity&)> callback) {
  instance_quit_callback_ = callback;
}

void ServiceManager::Connect(std::unique_ptr<ConnectParams> params) {
  Connect(std::move(params), nullptr);
}

void ServiceManager::RegisterService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  auto params = base::MakeUnique<ConnectParams>();

  if (!pid_receiver_request.is_pending()) {
    mojom::PIDReceiverPtr pid_receiver;
    pid_receiver_request = mojom::PIDReceiverRequest(&pid_receiver);
    pid_receiver->SetPID(base::Process::Current().Pid());
  }

  params->set_source(identity);
  params->set_target(identity);
  params->set_client_process_info(
      std::move(service), std::move(pid_receiver_request));
  Connect(std::move(params), nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManager, private:

void ServiceManager::InitCatalog(mojom::ServicePtr catalog) {
  // TODO(beng): It'd be great to build this from the manifest, however there's
  //             a bit of a chicken-and-egg problem.
  InterfaceProviderSpec spec;
  spec.provides["app"].insert("filesystem::mojom::Directory");
  spec.provides["catalog:catalog"].insert("catalog::mojom::Catalog");
  spec.provides["service_manager:resolver"].insert(
      "service_manager::mojom::Resolver");
  spec.provides["control"].insert("catalog::mojom::CatalogControl");
  InterfaceProviderSpecMap specs;
  specs[mojom::kServiceManager_ConnectorSpec] = spec;

  Instance* instance = CreateInstance(
      CreateServiceManagerIdentity(), CreateCatalogIdentity(), specs);
  singletons_.insert(catalog::mojom::kServiceName);
  instance->StartWithService(std::move(catalog));
}

mojom::Resolver* ServiceManager::GetResolver(const Identity& identity) {
  auto iter = identity_to_resolver_.find(identity);
  if (iter != identity_to_resolver_.end())
    return iter->second.get();

  mojom::ResolverPtr resolver_ptr;
  BindInterface(this, identity, CreateCatalogIdentity(), &resolver_ptr);
  mojom::Resolver* resolver = resolver_ptr.get();
  identity_to_resolver_[identity] = std::move(resolver_ptr);
  return resolver;
}

void ServiceManager::OnInstanceError(Instance* instance) {
  // We never clean up the ServiceManager's own instance.
  if (instance == service_manager_instance_)
    return;

  EraseInstanceIdentity(instance);
  auto it = instances_.find(instance);
  DCHECK(it != instances_.end());

  // Deletes |instance|.
  instances_.erase(it);
}

void ServiceManager::OnInstanceUnreachable(Instance* instance) {
  // If an Instance becomes unreachable, new connection requests for this
  // identity will elicit a new Instance instantiation. The unreachable instance
  // remains alive.
  EraseInstanceIdentity(instance);
}

void ServiceManager::OnInstanceStopped(const Identity& identity) {
  listeners_.ForAllPtrs([identity](mojom::ServiceManagerListener* listener) {
    listener->OnServiceStopped(identity);
  });
  if (!instance_quit_callback_.is_null())
    instance_quit_callback_.Run(identity);
}

void ServiceManager::Connect(std::unique_ptr<ConnectParams> params,
                             base::WeakPtr<Instance> source_instance) {
  TRACE_EVENT_INSTANT1("service_manager", "ServiceManager::Connect",
                       TRACE_EVENT_SCOPE_THREAD, "original_name",
                       params->target().name());
  DCHECK(!params->target().name().empty());
  DCHECK(base::IsValidGUID(params->target().user_id()));
  DCHECK_NE(mojom::kInheritUserID, params->target().user_id());
  DCHECK(!params->HasClientProcessInfo() ||
         !identity_to_instance_.count(params->target()));

  // Connect to an existing matching instance, if possible.
  if (!params->HasClientProcessInfo() && ConnectToExistingInstance(&params))
    return;

  // The catalog needs to see the source identity as that of the originating
  // app so it loads the correct store. Since the catalog is itself run as root
  // when this re-enters Connect() it'll be handled by
  // ConnectToExistingInstance().
  mojom::Resolver* resolver = GetResolver(Identity(
      service_manager::mojom::kServiceName, params->target().user_id()));

  std::string name = params->target().name();
  resolver->ResolveServiceName(
      name,
      base::Bind(&service_manager::ServiceManager::OnGotResolvedName,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&params),
                 !!source_instance, source_instance));
}

ServiceManager::Instance* ServiceManager::GetExistingInstance(
    const Identity& identity) const {
  const auto& it = identity_to_instance_.find(identity);
  Instance* instance = it != identity_to_instance_.end() ? it->second : nullptr;
  if (instance)
    return instance;

  if (singletons_.find(identity.name()) != singletons_.end()) {
    for (auto entry : identity_to_instance_) {
      if (entry.first.name() == identity.name() &&
          entry.first.instance() == identity.instance()) {
        return entry.second;
      }
    }
  }
  return nullptr;
}

void ServiceManager::EraseInstanceIdentity(Instance* instance) {
  const Identity& identity = instance->identity();

  auto it = identity_to_instance_.find(identity);
  if (it != identity_to_instance_.end()) {
    identity_to_instance_.erase(it);
    return;
  }

  if (singletons_.find(identity.name()) != singletons_.end()) {
    singletons_.erase(identity.name());
    for (auto it = identity_to_instance_.begin();
         it != identity_to_instance_.end(); ++it) {
      if (it->first.name() == identity.name() &&
          it->first.instance() == identity.instance()) {
        identity_to_instance_.erase(it);
        return;
      }
    }
  }
}

void ServiceManager::NotifyServiceStarted(const Identity& identity,
                                          base::ProcessId pid) {
  listeners_.ForAllPtrs(
      [identity, pid](mojom::ServiceManagerListener* listener) {
        listener->OnServiceStarted(identity, pid);
      });
}

void ServiceManager::NotifyServiceFailedToStart(const Identity& identity) {
  listeners_.ForAllPtrs(
      [identity](mojom::ServiceManagerListener* listener) {
        listener->OnServiceFailedToStart(identity);
      });
}

bool ServiceManager::ConnectToExistingInstance(
    std::unique_ptr<ConnectParams>* params) {
  Instance* instance = GetExistingInstance((*params)->target());
  if (instance) {
    if ((*params)->HasInterfaceRequestInfo()) {
      instance->CallOnBindInterface(params);
      return true;
    }
    return instance->CallOnConnect(params);
  }
  return false;
}

ServiceManager::Instance* ServiceManager::CreateInstance(
    const Identity& source,
    const Identity& target,
    const InterfaceProviderSpecMap& specs) {
  CHECK(target.user_id() != mojom::kInheritUserID);

  auto instance = base::MakeUnique<Instance>(this, target, specs);
  Instance* raw_instance = instance.get();

  instances_.insert(std::make_pair(raw_instance, std::move(instance)));

  // NOTE: |instance| has been passed elsewhere. Use |raw_instance| from this
  // point forward. It's safe for the extent of this method.

  auto result =
      identity_to_instance_.insert(std::make_pair(target, raw_instance));
  DCHECK(result.second);

  mojom::RunningServiceInfoPtr info = raw_instance->CreateRunningServiceInfo();
  listeners_.ForAllPtrs([&info](mojom::ServiceManagerListener* listener) {
    listener->OnServiceCreated(info.Clone());
  });

  return raw_instance;
}

void ServiceManager::AddListener(mojom::ServiceManagerListenerPtr listener) {
  // TODO(beng): filter instances provided by those visible to this service.
  std::vector<mojom::RunningServiceInfoPtr> instances;
  instances.reserve(identity_to_instance_.size());
  for (auto& instance : identity_to_instance_)
    instances.push_back(instance.second->CreateRunningServiceInfo());
  listener->OnInit(std::move(instances));

  listeners_.AddPtr(std::move(listener));
}

void ServiceManager::CreateServiceWithFactory(const Identity& service_factory,
                                              const std::string& name,
                                              mojom::ServiceRequest request) {
  mojom::ServiceFactory* factory = GetServiceFactory(service_factory);
  factory->CreateService(std::move(request), name);
}

mojom::ServiceFactory* ServiceManager::GetServiceFactory(
    const Identity& service_factory_identity) {
  auto it = service_factories_.find(service_factory_identity);
  if (it != service_factories_.end())
    return it->second.get();

  Identity source_identity(service_manager::mojom::kServiceName,
                           mojom::kInheritUserID);
  mojom::ServiceFactoryPtr factory;
  BindInterface(this, source_identity, service_factory_identity, &factory);
  mojom::ServiceFactory* factory_interface = factory.get();
  factory.set_connection_error_handler(
      base::Bind(&service_manager::ServiceManager::OnServiceFactoryLost,
                 weak_ptr_factory_.GetWeakPtr(), service_factory_identity));
  service_factories_[service_factory_identity] = std::move(factory);
  return factory_interface;
}

void ServiceManager::OnServiceFactoryLost(const Identity& which) {
  // Remove the mapping.
  auto it = service_factories_.find(which);
  DCHECK(it != service_factories_.end());
  service_factories_.erase(it);
}

void ServiceManager::OnGotResolvedName(std::unique_ptr<ConnectParams> params,
                                       bool has_source_instance,
                                       base::WeakPtr<Instance> source_instance,
                                       mojom::ResolveResultPtr result,
                                       mojom::ResolveResultPtr parent) {
  // If this request was originated by a specific Instance and that Instance is
  // no longer around, we ignore this response.
  if (has_source_instance && !source_instance)
    return;

  // If name resolution failed, we drop the connection.
  if (!result) {
    LOG(ERROR) << "Failed to resolve service name: " << params->target().name();
    RunCallback(params.get(), mojom::ConnectResult::INVALID_ARGUMENT, "");
    return;
  }

  std::string instance_name = params->target().instance();

  // |result->interface_provider_specs| can be empty when there is no manifest.
  InterfaceProviderSpec connection_spec = GetPermissiveInterfaceProviderSpec();
  auto it = result->interface_provider_specs.find(
      mojom::kServiceManager_ConnectorSpec);
  if (it != result->interface_provider_specs.end())
    connection_spec = it->second;

  const Identity original_target(params->target());
  const std::string user_id =
      HasCapability(connection_spec, kCapability_AllUsers)
          ? base::GenerateGUID() : params->target().user_id();
  const Identity target(params->target().name(), user_id, instance_name);
  params->set_target(target);

  // It's possible that when this manifest request was issued, another one was
  // already in-progress and completed by the time this one did, and so the
  // requested application may already be running.
  if (ConnectToExistingInstance(&params))
    return;

  Identity source = params->source();

  // Services that request "all_users" class from the Service Manager are
  // allowed to field connection requests from any user. They also run with a
  // synthetic user id generated here. The user id provided via Connect() is
  // ignored. Additionally services with the "all_users" class are not tied to
  // the lifetime of the service that started them, instead they are owned by
  // the Service Manager.
  Identity source_identity_for_creation;
  if (HasCapability(connection_spec, kCapability_AllUsers)) {
    singletons_.insert(target.name());
    source_identity_for_creation = CreateServiceManagerIdentity();
  } else {
    source_identity_for_creation = params->source();
  }

  Instance* instance = CreateInstance(source_identity_for_creation,
                                      target, result->interface_provider_specs);

  // Below are various paths through which a new Instance can be bound to a
  // Service proxy.
  if (params->HasClientProcessInfo()) {
    // This branch should be reachable only via a call to RegisterService() . We
    // start the instance but return early before we connect to it. Clients will
    // call Connect() with the target identity subsequently.
    instance->BindPIDReceiver(params->TakePIDReceiverRequest());
    instance->StartWithService(params->TakeService());
    return;
  } else {
    // Otherwise we create a new Service pipe.
    mojom::ServicePtr service;
    mojom::ServiceRequest request(&service);

    // The catalog was unable to read a manifest for this service. We can't do
    // anything more.
    // TODO(beng): There may be some cases where it's valid to have an empty
    // spec, so we should probably include a return value in |result|.
    if (result->interface_provider_specs.empty()) {
      LOG(ERROR)
          << "Error: The catalog was unable to read a manifest for service \""
          << result->name << "\".";
      RunCallback(params.get(), mojom::ConnectResult::ACCESS_DENIED, "");
      return;
    }

    if (parent) {
      // This service is provided by another service via a ServiceFactory.
      std::string target_user_id = target.user_id();
      std::string factory_instance_name = instance_name;

      auto spec_iter = parent->interface_provider_specs.find(
          mojom::kServiceManager_ConnectorSpec);
      if (spec_iter != parent->interface_provider_specs.end() &&
              HasCapability(spec_iter->second, kCapability_InstancePerChild)) {
        // If configured to start a new instance, create a random instance name
        // for the factory so that we don't reuse an existing process.
        factory_instance_name = base::GenerateGUID();
      } else {
        // Use the original user ID so the existing embedder factory can
        // be found and used to create the new service.
        target_user_id = original_target.user_id();
        Identity packaged_service_target(target);
        packaged_service_target.set_user_id(original_target.user_id());
        instance->set_identity(packaged_service_target);
      }
      instance->StartWithService(std::move(service));

      Identity factory(parent->name, target_user_id, factory_instance_name);
      CreateServiceWithFactory(factory, target.name(), std::move(request));
    } else {
      base::FilePath package_path;
      if (!service_overrides_ || !service_overrides_->GetExecutablePathOverride(
            target.name(), &package_path)) {
        package_path = result->package_path;
      }
      DCHECK(!package_path.empty());

      if (!instance->StartWithFilePath(package_path)) {
        OnInstanceError(instance);
        RunCallback(params.get(), mojom::ConnectResult::INVALID_ARGUMENT, "");
        return;
      }
    }
  }

  // Now that the instance has a Service, we can connect to it.
  if (params->HasInterfaceRequestInfo()) {
    instance->CallOnBindInterface(&params);
  } else {
    bool connected = instance->CallOnConnect(&params);
    DCHECK(connected);
  }
}

base::WeakPtr<ServiceManager> ServiceManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace service_manager
