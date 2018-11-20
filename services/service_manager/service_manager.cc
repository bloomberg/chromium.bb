// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_manager.h"

#include <stdint.h>

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/entry.h"
#include "services/catalog/instance.h"
#include "services/catalog/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if !defined(OS_IOS)
#include "services/service_manager/runner/host/service_process_launcher.h"
#endif

namespace service_manager {

namespace {

const char kCapability_ServiceManager[] = "service_manager:service_manager";

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
  DCHECK(target.IsValid());
  InterfaceSet exposed_interfaces;
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

const InterfaceProviderSpec& GetEmptyInterfaceProviderSpec() {
  static base::NoDestructor<InterfaceProviderSpec> spec;
  return *spec;
}

bool HasCapability(const InterfaceProviderSpec& spec,
                   const std::string& capability) {
  auto it = spec.requires.find(service_manager::mojom::kServiceName);
  if (it == spec.requires.end())
    return false;
  return it->second.find(capability) != it->second.end();
}

void ReportBlockedInterface(const std::string& source_service_name,
                            const std::string& target_service_name,
                            const std::string& target_interface_name) {
#if DCHECK_IS_ON()
  // While it would not be correct to assert that this never happens (e.g. a
  // compromised process may request invalid interfaces), we do want to
  // effectively treat all occurrences of this branch in production code as
  // bugs that must be fixed. This crash allows such bugs to be caught in
  // testing rather than relying on easily overlooked log messages.
  NOTREACHED()
#else
  LOG(ERROR)
#endif
      << "The Service Manager prevented service \"" << source_service_name
      << "\" from binding interface \"" << target_interface_name << "\""
      << " in target service \"" << target_service_name << "\". You probably "
      << "need to update one or more service manifests to ensure that \""
      << target_service_name << "\" exposes \"" << target_interface_name
      << "\" through a capability and that \"" << source_service_name
      << "\" requires that capability from the \"" << target_service_name
      << "\" service.";
}

void ReportBlockedStartService(const std::string& source_service_name,
                               const std::string& target_service_name) {
#if DCHECK_IS_ON()
  // See the note in ReportBlockedInterface above.
  NOTREACHED()
#else
  LOG(ERROR)
#endif
      << "Service \"" << source_service_name << "\" has attempted to manually "
      << "start service \"" << target_service_name << "\", but it is not "
      << "sufficiently privileged to do so. You probably need to update one or "
      << "services' manifests in order to remedy this situation.";
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
  if (!allowed)
    ReportBlockedInterface(source.name(), target.name(), interface_name);
  return allowed;
}

}  // namespace

const Identity& GetServiceManagerInstanceIdentity() {
  static base::NoDestructor<Identity> id{service_manager::mojom::kServiceName,
                                         kSystemInstanceGroup, base::Token{},
                                         base::Token::CreateRandom()};
  return *id;
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
           const InterfaceProviderSpecMap& interface_provider_specs,
           const catalog::ServiceOptions& options)
      : service_manager_(service_manager),
        id_(GenerateUniqueID()),
        identity_(identity),
        interface_provider_specs_(interface_provider_specs),
        options_(options),
        allow_any_application_(GetConnectionSpec().requires.count("*") == 1),
        pid_receiver_binding_(this),
        control_binding_(this),
        state_(State::IDLE),
        weak_factory_(this) {
    if (identity_.name() == service_manager::mojom::kServiceName ||
        identity_.name() == catalog::mojom::kServiceName) {
      pid_ = GetCurrentPid();
    }
    DCHECK_GT(id_, 0u);
    DCHECK(identity_.IsValid());
  }

  void Stop() {
    DCHECK_NE(state_, State::STOPPED);

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

    state_ = State::STOPPED;
  }

  ~Instance() override {
    // The instance may have already been stopped prior to destruction if the
    // ServiceManager itself is being torn down.
    if (state_ != State::STOPPED)
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
    if (!source)
      return false;

    const InterfaceProviderSpec& source_spec = source->GetConnectionSpec();
    if (!AllowsInterface(params->source(), source_spec, identity_,
                         GetConnectionSpec(), params->interface_name())) {
      params->set_response_data(mojom::ConnectResult::ACCESS_DENIED, identity_);
      return false;
    }

    params->set_response_data(mojom::ConnectResult::SUCCEEDED, identity_);

    pending_service_connections_++;
    service_->OnBindInterface(
        BindSourceInfo(params->source(),
                       GetRequestedCapabilities(source_spec, identity_)),
        params->interface_name(), params->TakeInterfaceRequestPipe(),
        base::BindOnce(&Instance::OnConnectComplete, base::Unretained(this)));
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
        base::BindOnce(&Instance::OnServiceLost, base::Unretained(this),
                       service_manager_->GetWeakPtr()));
    service_->OnStart(identity_, base::BindOnce(&Instance::OnStartComplete,
                                                base::Unretained(this)));
  }

  bool StartWithFilePath(const base::FilePath& path, SandboxType sandbox_type) {
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
    // TODO(tsepez): use actual sandbox type. https://crbug.com/788778
    mojom::ServicePtr service = runner_->Start(
        identity_, SANDBOX_TYPE_NO_SANDBOX,
        base::BindOnce(&Instance::PIDAvailable, weak_factory_.GetWeakPtr()));
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

  void set_identity(const Identity& identity) {
    DCHECK(identity.IsValid());
    identity_ = identity;
  }

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
    STARTED,

    // The service has been stopped.
    STOPPED,
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
      DCHECK(source_identity_.IsValid());
      DCHECK(target_identity_.IsValid());
      target_.set_connection_error_handler(base::BindOnce(
          &InterfaceProviderImpl::OnConnectionError, base::Unretained(this)));
      source_binding_.set_connection_error_handler(base::BindOnce(
          &InterfaceProviderImpl::OnConnectionError, base::Unretained(this)));
    }

    ~InterfaceProviderImpl() override = default;

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
  void BindInterface(const service_manager::ServiceFilter& target_filter,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     BindInterfaceCallback callback) override {
    mojom::ConnectResult result =
        ValidateConnectParams(target_filter, interface_name);
    if (result != mojom::ConnectResult::SUCCEEDED) {
      std::move(callback).Run(result, base::nullopt);
      return;
    }

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target_filter);
    params->set_interface_request_info(interface_name,
                                       std::move(interface_pipe));
    params->set_connection_callback(std::move(callback));
    service_manager_->Connect(std::move(params));
  }

  void QueryService(const std::string& service_name,
                    QueryServiceCallback callback) override {
    std::string sandbox_type;
    bool success = service_manager_->QueryCatalog(
        service_name, identity_.instance_group(), &sandbox_type);
    if (success) {
      std::move(callback).Run(mojom::ServiceInfo::New(sandbox_type));
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  void WarmService(const ServiceFilter& target_filter,
                   WarmServiceCallback callback) override {
    mojom::ConnectResult result = ValidateConnectParams(
        target_filter, base::nullopt /* interface_name */);

    if (result != mojom::ConnectResult::SUCCEEDED) {
      std::move(callback).Run(result, base::nullopt);
      return;
    }

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target_filter);
    params->set_connection_callback(std::move(callback));
    service_manager_->Connect(std::move(params));
  }

  void RegisterServiceInstance(
      const Identity& identity,
      mojo::ScopedMessagePipeHandle service_handle,
      mojom::PIDReceiverRequest pid_receiver_request,
      RegisterServiceInstanceCallback callback) override {
    mojom::ServicePtr service(
        mojom::ServicePtrInfo(std::move(service_handle), 0));

    if (!options_.can_create_other_service_instances) {
      LOG(ERROR) << "Instance: " << identity_.name() << " attempting "
                 << "to register an instance for a process it created for "
                 << "target: " << identity.name() << " without "
                 << "the 'can_create_other_service_instances' option.";
      std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED);
      return;
    }

    if (service_manager_->GetExistingInstance(identity)) {
      LOG(ERROR) << "Instance already exists: " << identity.ToString();
      std::move(callback).Run(mojom::ConnectResult::INVALID_ARGUMENT);
      return;
    }

    auto target_filter = ServiceFilter::ForExactIdentity(identity);
    mojom::ConnectResult result = ValidateConnectParams(
        target_filter, base::nullopt /* interface_name */);
    if (result != mojom::ConnectResult::SUCCEEDED) {
      std::move(callback).Run(result);
      return;
    }

    auto params = std::make_unique<ConnectParams>();
    params->set_source(identity_);
    params->set_target(target_filter);
    params->set_client_process_info(std::move(service),
                                    std::move(pid_receiver_request));
    params->set_connection_callback(base::BindOnce(
        [](RegisterServiceInstanceCallback callback,
           mojom::ConnectResult result,
           const base::Optional<Identity>& identity) {
          std::move(callback).Run(result);
        },
        std::move(callback)));
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
    // TODO(beng): this should only track the instances matching this instance
    // group, or the root instance group.
    service_manager_->AddListener(std::move(listener));
  }

  mojom::ConnectResult ValidateConnectParams(
      const ServiceFilter& target_filter,
      const base::Optional<std::string>& target_interface_name) {
    if (target_filter.service_name().empty()) {
      DLOG(ERROR) << "ServiceFilter has no service name.";
      return mojom::ConnectResult::INVALID_ARGUMENT;
    }

    const InterfaceProviderSpec& connection_spec = GetConnectionSpec();

    // TODO(beng): Need to do the following additional policy validation of
    // whether this instance is allowed to connect using:
    // - non-null client process info.
    bool skip_instance_group_check =
        options_.instance_sharing ==
            catalog::ServiceOptions::InstanceSharingType::SINGLETON ||
        options_.instance_sharing ==
            catalog::ServiceOptions::InstanceSharingType::
                SHARED_ACROSS_INSTANCE_GROUPS ||
        options_.can_connect_to_instances_in_any_group;

    if (!skip_instance_group_check && target_filter.instance_group() &&
        target_filter.instance_group() != identity_.instance_group() &&
        target_filter.instance_group() != kSystemInstanceGroup) {
      LOG(ERROR) << "Instance " << identity_.ToString() << " attempting to "
                 << "connect to " << target_filter.service_name() << " in "
                 << "group " << target_filter.instance_group()->ToString()
                 << " without the 'can_connect_to_instances_in_any_group' "
                 << "option.";
      return mojom::ConnectResult::ACCESS_DENIED;
    }
    if (target_filter.instance_id() &&
        !target_filter.instance_id()->is_zero() &&
        !options_.can_connect_to_other_services_with_any_instance_name) {
      LOG(ERROR)
          << "Instance " << identity_.ToString() << " attempting to connect to "
          << target_filter.service_name() << " with instance ID "
          << target_filter.instance_id()->ToString() << " without the "
          << "'can_connect_to_other_services_with_any_instance_name' option.";
      return mojom::ConnectResult::ACCESS_DENIED;
    }

    if (allow_any_application_ ||
        connection_spec.requires.find(target_filter.service_name()) !=
            connection_spec.requires.end()) {
      return mojom::ConnectResult::SUCCEEDED;
    }

    if (target_interface_name) {
      ReportBlockedInterface(identity_.name(), target_filter.service_name(),
                             *target_interface_name);
    } else {
      ReportBlockedStartService(identity_.name(), target_filter.service_name());
    }

    return mojom::ConnectResult::ACCESS_DENIED;
  }

  uint32_t GenerateUniqueID() const {
    static uint32_t id = 0;
    ++id;
    CHECK_NE(0u, id);
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
    service_manager_->NotifyServicePIDReceived(identity_, pid_);
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
      connectors_.set_connection_error_handler(base::BindRepeating(
          &Instance::OnConnectionLost, base::Unretained(this),
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
  const catalog::ServiceOptions options_;
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
    Instance* instance =
        service_manager_->GetExistingInstance(source_info.identity);
    DCHECK(instance);
    instance->OnBindInterface(source_info, interface_name,
                              std::move(interface_pipe));
  }

 private:
  ServiceManager* const service_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

// A container of Instances that stores them with an Identity and an
// InstanceType so they can be resolved based on part of that Identity. See
// comments on the InstanceType definition for more on how different types of
// service identities are resolved.
class ServiceManager::IdentityToInstanceMap {
 public:
  // An entry in any of the mappings owned by this object.
  struct Entry {
    Entry(const base::Token& guid, Instance* instance)
        : guid(guid), instance(instance) {
      DCHECK(!guid.is_zero());
      DCHECK(instance);
    }
    Entry(const Entry&) = default;
    ~Entry() = default;

    base::Token guid;
    Instance* instance = nullptr;
  };

  struct RegularInstanceKey {
    RegularInstanceKey(const std::string& service_name,
                       const base::Token& instance_group,
                       const base::Token& instance_id)
        : service_name(service_name),
          instance_group(instance_group),
          instance_id(instance_id) {}
    RegularInstanceKey(const RegularInstanceKey&) = default;
    ~RegularInstanceKey() = default;

    bool operator==(const RegularInstanceKey& other) const {
      return service_name == other.service_name &&
             instance_group == other.instance_group &&
             instance_id == other.instance_id;
    }

    bool operator<(const RegularInstanceKey& other) const {
      return std::tie(service_name, instance_group, instance_id) <
             std::tie(other.service_name, other.instance_group,
                      other.instance_id);
    }

    const std::string service_name;
    const base::Token instance_group;
    const base::Token instance_id;
  };

  struct SharedInstanceKey {
    SharedInstanceKey(const std::string& service_name,
                      const base::Token& instance_id)
        : service_name(service_name), instance_id(instance_id) {}
    SharedInstanceKey(const SharedInstanceKey&) = default;
    ~SharedInstanceKey() = default;

    bool operator==(const SharedInstanceKey& other) const {
      return service_name == other.service_name &&
             instance_id == other.instance_id;
    }

    bool operator<(const SharedInstanceKey& other) const {
      if (service_name != other.service_name)
        return service_name < other.service_name;
      return instance_id < other.instance_id;
    }

    const std::string service_name;
    const base::Token instance_id;
  };

  IdentityToInstanceMap() = default;
  ~IdentityToInstanceMap() = default;

  void Insert(const Identity& identity, InstanceType type, Instance* instance) {
    DCHECK(identity.IsValid());
    DCHECK_NE(instance, nullptr);
    DCHECK_EQ(Get(identity), nullptr);
    switch (type) {
      case InstanceType::kRegular: {
        const RegularInstanceKey key{identity.name(), identity.instance_group(),
                                     identity.instance_id()};
        regular_instances_[key].emplace_back(identity.globally_unique_id(),
                                             instance);
        break;
      }

      case InstanceType::kSharedAcrossInstanceGroups: {
        const SharedInstanceKey key{identity.name(), identity.instance_id()};
        shared_instances_[key].emplace_back(identity.globally_unique_id(),
                                            instance);
        break;
      }

      case InstanceType::kSingleton:
        singleton_instances_[identity.name()].emplace_back(
            identity.globally_unique_id(), instance);
        break;

      default:
        NOTREACHED();
    }
  }

  Instance* Get(const ServiceFilter& filter) {
    DCHECK(filter.instance_group());
    DCHECK(filter.instance_id());
    DCHECK(!filter.globally_unique_id() ||
           !filter.globally_unique_id()->is_zero());

    const RegularInstanceKey regular_key{
        filter.service_name(), *filter.instance_group(), *filter.instance_id()};
    auto regular_iter = regular_instances_.find(regular_key);
    if (regular_iter != regular_instances_.end()) {
      return FindMatchingInstance(regular_iter->second,
                                  filter.globally_unique_id());
    }

    const SharedInstanceKey shared_key{filter.service_name(),
                                       *filter.instance_id()};
    auto shared_iter = shared_instances_.find(
        SharedInstanceKey(filter.service_name(), *filter.instance_id()));
    if (shared_iter != shared_instances_.end()) {
      return FindMatchingInstance(shared_iter->second,
                                  filter.globally_unique_id());
    }

    auto singleton_iter = singleton_instances_.find(filter.service_name());
    if (singleton_iter != singleton_instances_.end()) {
      return FindMatchingInstance(singleton_iter->second,
                                  filter.globally_unique_id());
    }

    return nullptr;
  }

  bool Erase(const Identity& identity) {
    DCHECK(identity.IsValid());

    const RegularInstanceKey regular_key{
        identity.name(), identity.instance_group(), identity.instance_id()};
    auto regular_iter = regular_instances_.find(regular_key);
    if (regular_iter != regular_instances_.end()) {
      auto& entries = regular_iter->second;
      if (EraseEntry(identity.globally_unique_id(), &entries)) {
        if (entries.empty())
          regular_instances_.erase(regular_iter);
        return true;
      }
    }

    const SharedInstanceKey shared_key{identity.name(), identity.instance_id()};
    auto shared_iter = shared_instances_.find(shared_key);
    if (shared_iter != shared_instances_.end()) {
      auto& entries = shared_iter->second;
      if (EraseEntry(identity.globally_unique_id(), &entries)) {
        if (entries.empty())
          shared_instances_.erase(shared_iter);
        return true;
      }
    }

    auto singleton_iter = singleton_instances_.find(identity.name());
    if (singleton_iter != singleton_instances_.end()) {
      auto& entries = singleton_iter->second;
      if (EraseEntry(identity.globally_unique_id(), &entries)) {
        if (entries.empty())
          singleton_instances_.erase(singleton_iter);
        return true;
      }
    }

    return false;
  }

  void PopulateRunningServiceInfo(
      std::vector<mojom::RunningServiceInfoPtr>* running_service_info) {
    running_service_info->reserve(regular_instances_.size() +
                                  shared_instances_.size() +
                                  singleton_instances_.size());
    for (auto& iter : regular_instances_)
      CreateServiceInfos(iter.second, running_service_info);
    for (auto& iter : shared_instances_)
      CreateServiceInfos(iter.second, running_service_info);
    for (auto& iter : singleton_instances_)
      CreateServiceInfos(iter.second, running_service_info);
  }

 private:
  // Maps a 3-tuple of (service name, instance group, instance ID) to a list of
  // service instances and their GUIDs.
  using RegularInstanceMap = std::map<RegularInstanceKey, std::vector<Entry>>;

  // Maps a 2-tuple of (service name, instance ID) to a list of shared service
  // instances and their GUIDs.
  using SharedInstanceMap = std::map<SharedInstanceKey, std::vector<Entry>>;

  // Maps a service name to a list of singleton instances and their GUIDs.
  using SingletonInstanceMap = std::map<std::string, std::vector<Entry>>;

  Instance* FindMatchingInstance(const std::vector<Entry>& entries,
                                 const base::Optional<base::Token>& guid) {
    DCHECK(!entries.empty());
    if (!guid.has_value())
      return entries.front().instance;

    for (const auto& entry : entries) {
      if (entry.guid == *guid)
        return entry.instance;
    }

    return nullptr;
  }

  bool EraseEntry(const base::Token& guid, std::vector<Entry>* entries) {
    auto it = std::find_if(
        entries->begin(), entries->end(),
        [&guid](const Entry& entry) { return entry.guid == guid; });
    if (it == entries->end())
      return false;

    entries->erase(it);
    return true;
  }

  void CreateServiceInfos(const std::vector<Entry>& entries,
                          std::vector<mojom::RunningServiceInfoPtr>* infos) {
    for (const auto& entry : entries)
      infos->push_back(entry.instance->CreateRunningServiceInfo());
  }

  RegularInstanceMap regular_instances_;
  SharedInstanceMap shared_instances_;
  SingletonInstanceMap singleton_instances_;

  DISALLOW_COPY_AND_ASSIGN(IdentityToInstanceMap);
};

ServiceManager::ServiceManager(std::unique_ptr<ServiceProcessLauncherFactory>
                                   service_process_launcher_factory,
                               std::unique_ptr<base::Value> catalog_contents,
                               catalog::ManifestProvider* manifest_provider)
    : catalog_(std::move(catalog_contents), manifest_provider),
      identity_to_instance_(std::make_unique<IdentityToInstanceMap>()),
      service_process_launcher_factory_(
          std::move(service_process_launcher_factory)),
      weak_ptr_factory_(this) {
  InterfaceProviderSpec spec;
  spec.provides[kCapability_ServiceManager].insert(
      "service_manager.mojom.ServiceManager");
  spec.requires["*"].insert("service_manager:service_factory");
  InterfaceProviderSpecMap specs;
  specs[mojom::kServiceManager_ConnectorSpec] = std::move(spec);

  service_manager_instance_ = CreateInstance(
      GetServiceManagerInstanceIdentity(), InstanceType::kSingleton,
      std::move(specs), catalog::ServiceOptions());

  mojom::ServicePtr service;
  service_context_.reset(new ServiceContext(std::make_unique<ServiceImpl>(this),
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
  for (const auto& instance : instances_) {
    if (instance.first != service_manager_instance_)
      instance.first->Stop();
  }

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
                       params->target().service_name());
  const Identity& source = params->source();
  DCHECK(source.IsValid());

  ServiceFilter target_filter = params->target();

  // If the target filter does not specify an instance group, we assume the
  // source's own.
  if (!target_filter.instance_group())
    target_filter.set_instance_group(source.instance_group());

  // If the target filter does not specify an instance ID, we assume zero.
  if (!target_filter.instance_id())
    target_filter.set_instance_id(base::Token{});

  if (!params->HasClientProcessInfo()) {
    // Connect to an existing matching instance, if possible.
    Instance* instance = identity_to_instance_->Get(target_filter);
    if (instance) {
      if (params->HasInterfaceRequestInfo()) {
        instance->CallOnBindInterface(&params);
      } else {
        // This is a StartService request and the instance is already running.
        // Make sure the response identity is properly resolved.
        params->set_response_data(mojom::ConnectResult::SUCCEEDED,
                                  instance->identity());
      }
      return;
    }

    // If there was no existing instance but the request specified a specific
    // globally unique ID for the target, ignore the request. That instance is
    // obviously no longer running.
    if (target_filter.globally_unique_id())
      return;
  }

  // Beyond this point, in order to fulfill the connection request we need to
  // start a new instance of the target service.

  const catalog::Entry* entry =
      catalog_.GetInstanceForGroup(*target_filter.instance_group())
          ->Resolve(target_filter.service_name());
  if (!entry) {
    LOG(ERROR) << "Failed to resolve service name: "
               << target_filter.service_name();
    params->set_response_data(mojom::ConnectResult::INVALID_ARGUMENT,
                              base::nullopt);
    return;
  }

  const InterfaceProviderSpecMap& interface_provider_specs =
      entry->interface_provider_specs();

  const catalog::ServiceOptions& options = entry->options();

  bool all_user_instance = entry->options().instance_sharing ==
                           catalog::ServiceOptions::InstanceSharingType::
                               SHARED_ACROSS_INSTANCE_GROUPS;
  bool singleton_instance =
      entry->options().instance_sharing ==
      catalog::ServiceOptions::InstanceSharingType::SINGLETON;

  // Services that have "shared_across_instance_groups" value of
  // "instance_sharing" option are allowed to field connection requests from
  // instances in any instance group. They also run with a synthetic instance
  // group generated here. The instance group provided via |Connect()| is
  // ignored.

  InstanceType instance_type;
  if (singleton_instance)
    instance_type = InstanceType::kSingleton;
  else if (all_user_instance)
    instance_type = InstanceType::kSharedAcrossInstanceGroups;
  else
    instance_type = InstanceType::kRegular;

  Identity new_instance_identity;
  if (params->HasClientProcessInfo()) {
    // This is a service instance registration, so we should use the exact
    // Identity given by the caller.
    if (!target_filter.globally_unique_id() ||
        target_filter.globally_unique_id()->is_zero()) {
      params->set_response_data(mojom::ConnectResult::INVALID_ARGUMENT,
                                base::nullopt);
      return;
    }
    new_instance_identity = Identity(
        target_filter.service_name(), *target_filter.instance_group(),
        *target_filter.instance_id(), *target_filter.globally_unique_id());
  } else if (instance_type == InstanceType::kSingleton) {
    // For singleton instances, we generate a random group ID along with the
    // random GUID.
    new_instance_identity =
        Identity(target_filter.service_name(), base::Token::CreateRandom(),
                 base::Token{}, base::Token::CreateRandom());
  } else if (instance_type == InstanceType::kSharedAcrossInstanceGroups) {
    // For services shared across instance groups, we respect the target
    // instance ID but still generate a random group ID along with the random
    // GUID.
    new_instance_identity =
        Identity(target_filter.service_name(), base::Token::CreateRandom(),
                 *target_filter.instance_id(), base::Token::CreateRandom());
  } else {
    DCHECK_EQ(instance_type, InstanceType::kRegular);
    new_instance_identity =
        Identity(target_filter.service_name(), *target_filter.instance_group(),
                 *target_filter.instance_id(), base::Token::CreateRandom());
  }

  // The catalog was unable to read a manifest for this service. We can't do
  // anything more.
  if (interface_provider_specs.empty()) {
    LOG(ERROR)
        << "Error: The catalog was unable to read a manifest for service \""
        << entry->name() << "\".";
    params->set_response_data(mojom::ConnectResult::ACCESS_DENIED,
                              base::nullopt);
    return;
  }

  Instance* instance = CreateInstance(new_instance_identity, instance_type,
                                      interface_provider_specs, options);

  // Below are various paths through which a new Instance can be bound to a
  // Service proxy.
  if (params->HasClientProcessInfo()) {
    // If this is a service registration via |RegisterService()| or
    // |Connector.RegisterServiceInstance()|, we're done.
    instance->BindPIDReceiver(params->TakePIDReceiverRequest());
    instance->StartWithService(params->TakeService());
    return;
  }

  if (entry->parent()) {
    // This service is provided by another service via a ServiceFactory.
    //
    // We normally ignore the target instance group and generate a unique
    // instance group identifier when starting shared instances, but when those
    // instances need to be started by a service factory, it's conceivable that
    // the factory itself may be part of the originally targeted instance group.
    // In that case we use the originally targeted instance group to identify
    // the factory service.
    //
    // TODO(https://904240): This is super weird and hard to rationalize. Maybe
    // it's the wrong thing to do.
    instance->set_identity(
        Identity(new_instance_identity.name(), *target_filter.instance_group(),
                 new_instance_identity.instance_id(),
                 new_instance_identity.globally_unique_id()));

    auto factory_filter = ServiceFilter::ByNameWithIdInGroup(
        entry->parent()->name(), *target_filter.instance_id(),
        *target_filter.instance_group());

    mojom::PIDReceiverPtr pid_receiver;
    instance->BindPIDReceiver(mojo::MakeRequest(&pid_receiver));

    mojom::ServicePtr service;
    CreateServiceWithFactory(factory_filter, target_filter.service_name(),
                             mojo::MakeRequest(&service),
                             std::move(pid_receiver));
    instance->StartWithService(std::move(service));
  } else {
    base::FilePath package_path = entry->path();
    DCHECK(!package_path.empty());
    if (!instance->StartWithFilePath(
            package_path,
            UtilitySandboxTypeFromString(entry->sandbox_type()))) {
      OnInstanceError(instance);
      params->set_response_data(mojom::ConnectResult::INVALID_ARGUMENT,
                                base::nullopt);
      return;
    }
  }

  params->set_response_data(mojom::ConnectResult::SUCCEEDED,
                            instance->identity());
  if (params->HasInterfaceRequestInfo())
    instance->CallOnBindInterface(&params);
}

void ServiceManager::StartService(const std::string& service_name) {
  auto params = std::make_unique<ConnectParams>();
  params->set_source(GetServiceManagerInstanceIdentity());
  params->set_target(
      ServiceFilter::ByNameInGroup(service_name, kSystemInstanceGroup));
  Connect(std::move(params));
}

bool ServiceManager::QueryCatalog(const std::string& service_name,
                                  const base::Token& instance_group,
                                  std::string* sandbox_type) {
  const catalog::Entry* entry =
      catalog_.GetInstanceForGroup(instance_group)->Resolve(service_name);
  if (!entry)
    return false;
  *sandbox_type = entry->sandbox_type();
  return true;
}

void ServiceManager::RegisterService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  auto params = std::make_unique<ConnectParams>();

  if (!pid_receiver_request.is_pending()) {
    mojom::PIDReceiverPtr pid_receiver;
    pid_receiver_request = mojo::MakeRequest(&pid_receiver);
    pid_receiver->SetPID(GetCurrentPid());
  }

  DCHECK(identity.IsValid());
  params->set_source(identity);
  params->set_target(ServiceFilter::ForExactIdentity(identity));
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
  spec.provides["directory"].insert("filesystem.mojom.Directory");
  spec.provides["catalog:catalog"].insert("catalog.mojom.Catalog");
  spec.provides["control"].insert("catalog.mojom.CatalogControl");
  InterfaceProviderSpecMap specs;
  specs[mojom::kServiceManager_ConnectorSpec] = std::move(spec);

  Identity id{catalog::mojom::kServiceName, kSystemInstanceGroup, base::Token{},
              base::Token::CreateRandom()};
  Instance* instance =
      CreateInstance(id, InstanceType::kSingleton, std::move(specs),
                     catalog::ServiceOptions());
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
  listeners_.ForAllPtrs([&identity](mojom::ServiceManagerListener* listener) {
    listener->OnServiceStopped(identity);
  });
  if (!instance_quit_callback_.is_null())
    instance_quit_callback_.Run(identity);
}

ServiceManager::Instance* ServiceManager::GetExistingInstance(
    const Identity& identity) const {
  return identity_to_instance_->Get(ServiceFilter::ForExactIdentity(identity));
}

void ServiceManager::EraseInstanceIdentity(Instance* instance) {
  identity_to_instance_->Erase(instance->identity());
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

ServiceManager::Instance* ServiceManager::CreateInstance(
    const Identity& identity,
    InstanceType instance_type,
    const InterfaceProviderSpecMap& specs,
    const catalog::ServiceOptions& options) {
  DCHECK(identity.IsValid());

  auto instance = std::make_unique<Instance>(this, identity, specs, options);
  Instance* raw_instance = instance.get();

  instances_.insert(std::make_pair(raw_instance, std::move(instance)));

  // NOTE: |instance| has been passed elsewhere. Use |raw_instance| from this
  // point forward. It's safe for the extent of this method.

  identity_to_instance_->Insert(identity, instance_type, raw_instance);

  mojom::RunningServiceInfoPtr info = raw_instance->CreateRunningServiceInfo();
  listeners_.ForAllPtrs([&info](mojom::ServiceManagerListener* listener) {
    listener->OnServiceCreated(info.Clone());
  });

  return raw_instance;
}

void ServiceManager::AddListener(mojom::ServiceManagerListenerPtr listener) {
  // TODO(beng): filter instances provided by those visible to this service.
  std::vector<mojom::RunningServiceInfoPtr> instances;
  identity_to_instance_->PopulateRunningServiceInfo(&instances);
  listener->OnInit(std::move(instances));
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
  auto params = std::make_unique<ConnectParams>();
  params->set_source(GetServiceManagerInstanceIdentity());
  params->set_target(filter);
  params->set_interface_request_info(
      service_manager::mojom::ServiceFactory::Name_,
      mojo::MakeRequest(&factory).PassMessagePipe());
  Connect(std::move(params));

  mojom::ServiceFactory* factory_interface = factory.get();
  factory.set_connection_error_handler(
      base::BindOnce(&service_manager::ServiceManager::OnServiceFactoryLost,
                     weak_ptr_factory_.GetWeakPtr(), filter));
  service_factories_[filter] = std::move(factory);
  return factory_interface;
}

void ServiceManager::OnServiceFactoryLost(const ServiceFilter& which) {
  // Remove the mapping.
  auto it = service_factories_.find(which);
  DCHECK(it != service_factories_.end());
  service_factories_.erase(it);
}

base::WeakPtr<ServiceManager> ServiceManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace service_manager
