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
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/entry.h"
#include "services/catalog/instance.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/service_manager/connect_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/public/interfaces/service_control.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"

namespace service_manager {

namespace {

const char kCapability_UserID[] = "service_manager:user_id";
const char kCapability_ClientProcess[] = "service_manager:client_process";
const char kCapability_InstanceName[] = "service_manager:instance_name";
const char kCapability_AllUsers[] = "service_manager:all_users";
const char kCapability_ServiceManager[] = "service_manager:service_manager";

bool Succeeded(mojom::ConnectResult result) {
  return result == mojom::ConnectResult::SUCCEEDED;
}

// Returns the set of capabilities required from the target.
CapabilitySet GetRequestedCapabilities(const InterfaceProviderSpec& source_spec,
                                       const Identity& target) {
  CapabilitySet capabilities;

  // Start by looking for specs specific to the supplied identity.
  auto it = source_spec.requires.find(target.name());
  if (it != source_spec.requires.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }

  // Apply wild card rules too.
  it = source_spec.requires.find("*");
  if (it != source_spec.requires.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }
  return capabilities;
}

base::ProcessId GetCurrentPid() {
#if defined(OS_IOS)
  // iOS does not support base::Process.
  return 0;
#else
  return base::Process::Current().Pid();
#endif
}

// Generates a single set of interfaces that is the union of all interfaces
// exposed by the target for the capabilities requested by the source.
InterfaceSet GetInterfacesToExpose(const InterfaceProviderSpec& source_spec,
                                   const Identity& target,
                                   const InterfaceProviderSpec& target_spec) {
  InterfaceSet exposed_interfaces;
  // TODO(beng): remove this once we can assert that an InterfaceRegistry must
  //             always be constructed with a valid identity.
  if (!target.IsValid()) {
    exposed_interfaces.insert("*");
    return exposed_interfaces;
  }
  CapabilitySet capabilities = GetRequestedCapabilities(source_spec, target);
  for (const auto& capability : capabilities) {
    auto it = target_spec.provides.find(capability);
    if (it != target_spec.provides.end()) {
      for (const auto& interface_name : it->second)
        exposed_interfaces.insert(interface_name);
    }
  }
  return exposed_interfaces;
}

Identity CreateCatalogIdentity() {
  return Identity(catalog::mojom::kServiceName, mojom::kRootUserID);
}

InterfaceProviderSpec CreatePermissiveInterfaceProviderSpec() {
  InterfaceProviderSpec spec;
  InterfaceSet interfaces;
  interfaces.insert("*");
  spec.requires["*"] = std::move(interfaces);
  return spec;
}

const InterfaceProviderSpec& GetPermissiveInterfaceProviderSpec() {
  CR_DEFINE_STATIC_LOCAL(InterfaceProviderSpec, spec,
                         (CreatePermissiveInterfaceProviderSpec()));
  return spec;
}

const InterfaceProviderSpec& GetEmptyInterfaceProviderSpec() {
  CR_DEFINE_STATIC_LOCAL(InterfaceProviderSpec, spec, ());
  return spec;
}

bool HasCapability(const InterfaceProviderSpec& spec,
                   const std::string& capability) {
  auto it = spec.requires.find(service_manager::mojom::kServiceName);
  if (it == spec.requires.end())
    return false;
  return it->second.find(capability) != it->second.end();
}

bool AllowsInterface(const Identity& source,
                     const InterfaceProviderSpec& source_spec,
                     const Identity& target,
                     const InterfaceProviderSpec& target_spec,
                     const std::string& interface_name) {
  InterfaceSet exposed =
      GetInterfacesToExpose(source_spec, target, target_spec);
  bool allowed = (exposed.size() == 1 && exposed.count("*") == 1) ||
                 exposed.count(interface_name) > 0;
  if (!allowed) {
    std::stringstream ss;
    ss << "Connection InterfaceProviderSpec prevented service: "
       << source.name() << " from binding interface: " << interface_name
       << " exposed by: " << target.name();
    LOG(ERROR) << ss.str();
  }
  return allowed;
}

}  // namespace

Identity CreateServiceManagerIdentity() {
  return Identity(service_manager::mojom::kServiceName, mojom::kRootUserID);
}

// Encapsulates a connection to an instance of a service, tracked by the
// Service Manager.
class ServiceManager::Instance
    : public mojom::Connector,
      public mojom::PIDReceiver,
      public Service,
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
      pid_ = GetCurrentPid();
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

  bool CallOnBindInterface(std::unique_ptr<ConnectParams>* in_params) {
    if (!service_.is_bound()) {
      (*in_params)
          ->set_response_data(mojom::ConnectResult::ACCESS_DENIED, identity_);
      return false;
    }

    std::unique_ptr<ConnectParams> params(std::move(*in_params));
    Instance* source =
        service_manager_->GetExistingInstance(params->source());
    const InterfaceProviderSpec& source_connection_spec =
        source ? source->GetConnectionSpec() : GetEmptyInterfaceProviderSpec();

    if (!AllowsInterface(params->source(), source_connection_spec, identity_,
                         GetConnectionSpec(), params->interface_name())) {
      params->set_response_data(mojom::ConnectResult::ACCESS_DENIED, identity_);
      return false;
    }

    params->set_response_data(mojom::ConnectResult::SUCCEEDED, identity_);

    pending_service_connections_++;
    service_->OnBindInterface(
        BindSourceInfo(
            params->source(),
            GetRequestedCapabilities(source_connection_spec, identity_)),
        params->interface_name(), params->TakeInterfaceRequestPipe(),
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
    service_->OnStart(identity_, base::Bind(&Instance::OnStartComplete,
                                            base::Unretained(this)));
  }

  bool StartWithFilePath(const base::FilePath& path) {
#if defined(OS_IOS)
    // iOS does not support launching services in their own processes.
    NOTREACHED();
    return false;
#else
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
#endif
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
    return GetSpec(mojom::kServiceManager_ConnectorSpec);
  }
  bool HasSpec(const std::string& spec) const {
    auto it = interface_provider_specs_.find(spec);
    return it != interface_provider_specs_.end();
  }
  const InterfaceProviderSpec& GetSpec(const std::string& spec) const {
    auto it = interface_provider_specs_.find(spec);
    return it != interface_provider_specs_.end()
               ? it->second
               : GetEmptyInterfaceProviderSpec();
  }

  const Identity& identity() const { return identity_; }
  void set_identity(const Identity& identity) { identity_ = identity; }
  uint32_t id() const { return id_; }

  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    Instance* source =
        service_manager_->GetExistingInstance(source_info.identity);
    DCHECK(source);
    if (interface_name == mojom::ServiceManager::Name_ &&
        HasCapability(source->GetConnectionSpec(),
                      kCapability_ServiceManager)) {
      mojom::ServiceManagerRequest request =
          mojom::ServiceManagerRequest(std::move(interface_pipe));
      service_manager_bindings_.AddBinding(this, std::move(request));
    }
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

  class InterfaceProviderImpl : public mojom::InterfaceProvider {
   public:
    InterfaceProviderImpl(Instance* instance,
                          const std::string& spec,
                          const Identity& source_identity,
                          const Identity& target_identity,
                          service_manager::ServiceManager* service_manager,
                          mojom::InterfaceProviderPtr target,
                          mojom::InterfaceProviderRequest source_request)
        : instance_(instance),
          spec_(spec),
          source_identity_(source_identity),
          target_identity_(target_identity),
          service_manager_(service_manager),
          target_(std::move(target)),
          source_binding_(this, std::move(source_request)) {
      target_.set_connection_error_handler(base::Bind(
          &InterfaceProviderImpl::OnConnectionError, base::Unretained(this)));
      source_binding_.set_connection_error_handler(base::Bind(
          &InterfaceProviderImpl::OnConnectionError, base::Unretained(this)));
    }
    ~InterfaceProviderImpl() override {}

   private:
    // mojom::InterfaceProvider:
    void GetInterface(const std::string& interface_name,
                      mojo::ScopedMessagePipeHandle interface_pipe) override {
      Instance* source =
          service_manager_->GetExistingInstance(source_identity_);
      Instance* target =
          service_manager_->GetExistingInstance(target_identity_);
      if (!source || !target)
        return;
      if (!ValidateSpec(source) || !ValidateSpec(target))
        return;

      if (AllowsInterface(source_identity_, source->GetSpec(spec_),
                          target_identity_, target->GetSpec(spec_),
                          interface_name)) {
        target_->GetInterface(interface_name, std::move(interface_pipe));
      }
    }

    bool ValidateSpec(Instance* instance) const {
      if (!instance->HasSpec(spec_)) {
        LOG(ERROR) << "Instance for: " << instance->identity().name()
                   << " did not have spec named: " << spec_;
        return false;
      }
      return true;
    }

    void OnConnectionError() {
      // Deletes |this|.
      instance_->filters_.erase(this);
    }

    Instance* const instance_;
    const std::string spec_;
    const Identity source_identity_;
    const Identity target_identity_;
    const service_manager::ServiceManager* service_manager_;

    mojom::InterfaceProviderPtr target_;
    mojo::Binding<mojom::InterfaceProvider> source_binding_;

    DISALLOW_COPY_AND_ASSIGN(InterfaceProviderImpl);
  };

  // mojom::Connector implementation:
  void BindInterface(const service_manager::Identity& in_target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     const BindInterfaceCallback& callback) override {
    Identity target = in_target;
    mojom::ConnectResult result =
        ValidateConnectParams(&target, nullptr, nullptr);
    if (!Succeeded(result)) {
      callback.Run(result, Identity());
      return;
    }

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);
    params->set_interface_request_info(interface_name,
                                       std::move(interface_pipe));
    params->set_start_service_callback(callback);
    service_manager_->Connect(std::move(params));
  }

  void StartService(const Identity& in_target,
                    const StartServiceCallback& callback) override {
    Identity target = in_target;
    mojom::ConnectResult result =
        ValidateConnectParams(&target, nullptr, nullptr);
    if (!Succeeded(result)) {
      callback.Run(result, Identity());
      return;
    }

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);
    params->set_start_service_callback(callback);
    service_manager_->Connect(std::move(params));
  }

  void StartServiceWithProcess(
      const Identity& in_target,
      mojo::ScopedMessagePipeHandle service_handle,
      mojom::PIDReceiverRequest pid_receiver_request,
      const StartServiceWithProcessCallback& callback) override {
    Identity target = in_target;
    mojom::ConnectResult result =
        ValidateConnectParams(&target, nullptr, nullptr);
    if (!Succeeded(result)) {
      callback.Run(result, Identity());
      return;
    }

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);

    mojom::ServicePtr service;
    service.Bind(mojom::ServicePtrInfo(std::move(service_handle), 0));
    params->set_client_process_info(std::move(service),
                                    std::move(pid_receiver_request));
    params->set_start_service_callback(callback);
    service_manager_->Connect(std::move(params));
  }

  void Clone(mojom::ConnectorRequest request) override {
    connectors_.AddBinding(this, std::move(request));
  }

  void FilterInterfaces(const std::string& spec,
                        const Identity& source,
                        mojom::InterfaceProviderRequest source_request,
                        mojom::InterfaceProviderPtr target) override {
    auto* filter = new InterfaceProviderImpl(
        this, spec, source, identity_, service_manager_, std::move(target),
        std::move(source_request));
    filters_[filter] = base::WrapUnique(filter);
  }

  // mojom::PIDReceiver:
  void SetPID(uint32_t pid) override {
    PIDAvailable(pid);
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
    const InterfaceProviderSpec& connection_spec = GetConnectionSpec();
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
#if !defined(OS_IOS)
    // iOS does not support base::Process and simply passes 0 here, so elide
    // this check on that platform.
    if (pid == base::kNullProcessId) {
      service_manager_->OnInstanceError(this);
      return;
    }
#endif
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

  service_manager::ServiceManager* const service_manager_;

  // An id that identifies this instance. Distinct from pid, as a single process
  // may vend multiple application instances, and this object may exist before a
  // process is launched.
  const uint32_t id_;
  Identity identity_;
  const InterfaceProviderSpecMap interface_provider_specs_;
  const bool allow_any_application_;
#if !defined(OS_IOS)
  std::unique_ptr<ServiceProcessLauncher> runner_;
#endif
  mojom::ServicePtr service_;
  mojo::Binding<mojom::PIDReceiver> pid_receiver_binding_;
  mojo::BindingSet<mojom::Connector> connectors_;
  mojo::BindingSet<mojom::ServiceManager> service_manager_bindings_;
  mojo::AssociatedBinding<mojom::ServiceControl> control_binding_;
  base::ProcessId pid_ = base::kNullProcessId;
  State state_;

  std::map<InterfaceProviderImpl*, std::unique_ptr<InterfaceProviderImpl>>
      filters_;

  // The number of outstanding OnBindInterface requests which are in flight.
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
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    // The only interface ServiceManager exposes is mojom::ServiceManager, and
    // access to this interface is brokered by a policy specific to each caller,
    // managed by the caller's instance. Here we look to see who's calling,
    // and forward to the caller's instance to continue.
    Instance* instance = nullptr;
    for (const auto& entry : service_manager_->identity_to_instance_) {
      if (entry.first == source_info.identity) {
        instance = entry.second;
        break;
      }
    }

    DCHECK(instance);
    instance->OnBindInterface(source_info, interface_name,
                              std::move(interface_pipe));
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

ServiceManager::ServiceManager(std::unique_ptr<ServiceProcessLauncherFactory>
                                   service_process_launcher_factory,
                               std::unique_ptr<base::Value> catalog_contents,
                               catalog::ManifestProvider* manifest_provider)
    : catalog_(std::move(catalog_contents), manifest_provider),
      service_process_launcher_factory_(
          std::move(service_process_launcher_factory)),
      weak_ptr_factory_(this) {
  InterfaceProviderSpec spec;
  spec.provides[kCapability_ServiceManager].insert(
      "service_manager::mojom::ServiceManager");
  spec.requires["*"].insert("service_manager:service_factory");
  InterfaceProviderSpecMap specs;
  specs[mojom::kServiceManager_ConnectorSpec] = std::move(spec);

  service_manager_instance_ = CreateInstance(
      Identity(), CreateServiceManagerIdentity(), std::move(specs));
  singletons_.insert(service_manager::mojom::kServiceName);

  mojom::ServicePtr service;
  service_context_.reset(new ServiceContext(base::MakeUnique<ServiceImpl>(this),
                                            mojo::MakeRequest(&service)));
  service_manager_instance_->StartWithService(std::move(service));

  InitCatalog(catalog_.TakeService());
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

void ServiceManager::SetInstanceQuitCallback(
    base::Callback<void(const Identity&)> callback) {
  instance_quit_callback_ = std::move(callback);
}

void ServiceManager::Connect(std::unique_ptr<ConnectParams> params) {
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

  const catalog::Entry* entry =
      catalog_.GetInstanceForUserId(params->target().user_id())
          ->Resolve(params->target().name());
  if (!entry) {
    LOG(ERROR) << "Failed to resolve service name: " << params->target().name();
    params->set_response_data(mojom::ConnectResult::INVALID_ARGUMENT,
                              Identity());
    return;
  }

  const std::string& instance_name = params->target().instance();
  const InterfaceProviderSpecMap& interface_provider_specs =
      entry->interface_provider_specs();

  auto it = interface_provider_specs.find(mojom::kServiceManager_ConnectorSpec);
  const InterfaceProviderSpec& connection_spec =
      it != interface_provider_specs.end()
          ? it->second
          : GetPermissiveInterfaceProviderSpec();

  const Identity original_target(params->target());
  const std::string user_id =
      HasCapability(connection_spec, kCapability_AllUsers)
          ? base::GenerateGUID()
          : params->target().user_id();
  const Identity target(params->target().name(), user_id, instance_name);
  params->set_target(target);

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

  bool result_interface_provider_specs_empty = interface_provider_specs.empty();
  Instance* instance = CreateInstance(source_identity_for_creation, target,
                                      interface_provider_specs);

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
    // The catalog was unable to read a manifest for this service. We can't do
    // anything more.
    if (result_interface_provider_specs_empty) {
      LOG(ERROR)
          << "Error: The catalog was unable to read a manifest for service \""
          << entry->name() << "\".";
      params->set_response_data(mojom::ConnectResult::ACCESS_DENIED,
                                Identity());
      return;
    }

    if (entry->parent()) {
      // This service is provided by another service via a ServiceFactory.
      const std::string* target_user_id = &target.user_id();
      std::string factory_instance_name = instance_name;

      // Use the original user ID so the existing embedder factory can
      // be found and used to create the new service.
      target_user_id = &original_target.user_id();
      Identity packaged_service_target(target);
      packaged_service_target.set_user_id(original_target.user_id());
      instance->set_identity(packaged_service_target);

      mojom::ServicePtr service;
      Identity factory(entry->parent()->name(), *target_user_id,
                       factory_instance_name);
      CreateServiceWithFactory(factory, target.name(),
                               mojo::MakeRequest(&service));
      instance->StartWithService(std::move(service));
    } else {
      base::FilePath package_path = entry->path();
      DCHECK(!package_path.empty());
      if (!instance->StartWithFilePath(package_path)) {
        OnInstanceError(instance);
        params->set_response_data(mojom::ConnectResult::INVALID_ARGUMENT,
                                  Identity());
        return;
      }
    }
  }

  params->set_response_data(mojom::ConnectResult::SUCCEEDED,
                            instance->identity());
  if (params->HasInterfaceRequestInfo())
    instance->CallOnBindInterface(&params);
}

void ServiceManager::StartService(const Identity& identity) {
  auto params = base::MakeUnique<ConnectParams>();
  params->set_source(CreateServiceManagerIdentity());

  Identity target_identity = identity;
  if (target_identity.user_id() == mojom::kInheritUserID)
    target_identity.set_user_id(mojom::kRootUserID);
  params->set_target(target_identity);

  Connect(std::move(params));
}

void ServiceManager::RegisterService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  auto params = base::MakeUnique<ConnectParams>();

  if (!pid_receiver_request.is_pending()) {
    mojom::PIDReceiverPtr pid_receiver;
    pid_receiver_request = mojo::MakeRequest(&pid_receiver);
    pid_receiver->SetPID(GetCurrentPid());
  }

  params->set_source(identity);
  params->set_target(identity);
  params->set_client_process_info(
      std::move(service), std::move(pid_receiver_request));
  Connect(std::move(params));
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManager, private:

void ServiceManager::InitCatalog(mojom::ServicePtr catalog) {
  // TODO(beng): It'd be great to build this from the manifest, however there's
  //             a bit of a chicken-and-egg problem.
  InterfaceProviderSpec spec;
  spec.provides["app"].insert("filesystem::mojom::Directory");
  spec.provides["catalog:catalog"].insert("catalog::mojom::Catalog");
  spec.provides["control"].insert("catalog::mojom::CatalogControl");
  InterfaceProviderSpecMap specs;
  specs[mojom::kServiceManager_ConnectorSpec] = std::move(spec);

  Instance* instance =
      CreateInstance(CreateServiceManagerIdentity(), CreateCatalogIdentity(),
                     std::move(specs));
  singletons_.insert(catalog::mojom::kServiceName);
  instance->StartWithService(std::move(catalog));
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
  if (!instance)
    return false;

  if ((*params)->HasInterfaceRequestInfo())
    instance->CallOnBindInterface(params);
  return true;
}

ServiceManager::Instance* ServiceManager::CreateInstance(
    const Identity& source,
    const Identity& target,
    const InterfaceProviderSpecMap& specs) {
  CHECK(target.user_id() != mojom::kInheritUserID);

  auto instance = base::MakeUnique<Instance>(this, target, std::move(specs));
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

base::WeakPtr<ServiceManager> ServiceManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace service_manager
