// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/service_manager.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/connect_util.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/names.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/service.mojom.h"
#include "services/shell/public/interfaces/service_manager.mojom.h"

namespace shell {

namespace {

const char kCatalogName[] = "mojo:catalog";
const char kServiceManagerName[] = "mojo:shell";
const char kCapabilityClass_UserID[] = "shell:user_id";
const char kCapabilityClass_ClientProcess[] = "shell:client_process";
const char kCapabilityClass_InstanceName[] = "shell:instance_name";
const char kCapabilityClass_AllUsers[] = "shell:all_users";
const char kCapabilityClass_ExplicitClass[] = "shell:explicit_class";

}  // namespace

Identity CreateServiceManagerIdentity() {
  return Identity(kServiceManagerName, mojom::kRootUserID);
}

Identity CreateCatalogIdentity() {
  return Identity(kCatalogName, mojom::kRootUserID);
}

CapabilitySpec GetPermissiveCapabilities() {
  CapabilitySpec capabilities;
  CapabilityRequest spec;
  spec.interfaces.insert("*");
  capabilities.required["*"] = spec;
  return capabilities;
}

CapabilityRequest GetCapabilityRequest(const CapabilitySpec& source_spec,
                                       const Identity& target) {
  CapabilityRequest request;

  // Start by looking for specs specific to the supplied identity.
  auto it = source_spec.required.find(target.name());
  if (it != source_spec.required.end()) {
    std::copy(it->second.classes.begin(), it->second.classes.end(),
              std::inserter(request.classes, request.classes.begin()));
    std::copy(it->second.interfaces.begin(), it->second.interfaces.end(),
              std::inserter(request.interfaces, request.interfaces.begin()));
  }

  // Apply wild card rules too.
  it = source_spec.required.find("*");
  if (it != source_spec.required.end()) {
    std::copy(it->second.classes.begin(), it->second.classes.end(),
              std::inserter(request.classes, request.classes.begin()));
    std::copy(it->second.interfaces.begin(), it->second.interfaces.end(),
              std::inserter(request.interfaces, request.interfaces.begin()));
  }
  return request;
}

CapabilityRequest GenerateCapabilityRequestForConnection(
    const CapabilitySpec& source_spec,
    const Identity& target,
    const CapabilitySpec& target_spec) {
  CapabilityRequest request = GetCapabilityRequest(source_spec, target);
  // Flatten all interfaces from classes requested by the source into the
  // allowed interface set in the request.
  for (const auto& class_name : request.classes) {
    auto it = target_spec.provided.find(class_name);
    if (it != target_spec.provided.end()) {
      for (const auto& interface_name : it->second)
        request.interfaces.insert(interface_name);
    }
  }
  return request;
}

bool HasClass(const CapabilitySpec& spec, const std::string& class_name) {
  auto it = spec.required.find(kServiceManagerName);
  if (it == spec.required.end())
    return false;
  return it->second.classes.find(class_name) != it->second.classes.end();
}

// Encapsulates a connection to an instance of a service, tracked by the
// Service Manager.
class ServiceManager::Instance
    : public mojom::Connector,
      public mojom::PIDReceiver,
      public Service,
      public InterfaceFactory<mojom::ServiceManager>,
      public mojom::ServiceManager {
 public:
  Instance(shell::ServiceManager* service_manager,
           const Identity& identity,
           const CapabilitySpec& capability_spec)
      : service_manager_(service_manager),
        id_(GenerateUniqueID()),
        identity_(identity),
        capability_spec_(capability_spec),
        allow_any_application_(capability_spec.required.count("*") == 1),
        pid_receiver_binding_(this),
        weak_factory_(this) {
    if (identity_.name() == kServiceManagerName ||
        identity_.name() == kCatalogName) {
      pid_ = base::Process::Current().Pid();
    }
    DCHECK_NE(mojom::kInvalidInstanceID, id_);
  }

  ~Instance() override {
    if (parent_)
      parent_->RemoveChild(this);
    // |children_| will be modified during destruction.
    std::set<Instance*> children = children_;
    for (auto child : children)
      service_manager_->OnInstanceError(child);

    // Shutdown all bindings before we close the runner. This way the process
    // should see the pipes closed and exit, as well as waking up any potential
    // sync/WaitForIncomingResponse().
    service_.reset();
    if (pid_receiver_binding_.is_bound())
      pid_receiver_binding_.Close();
    connectors_.CloseAllBindings();
    service_manager_bindings_.CloseAllBindings();
    // Release |runner_| so that if we are called back to OnRunnerCompleted()
    // we know we're in the destructor.
    std::unique_ptr<NativeRunner> runner = std::move(runner_);
    runner.reset();
  }

  Instance* parent() { return parent_; }

  void AddChild(Instance* child) {
    children_.insert(child);
    child->parent_ = this;
  }

  void RemoveChild(Instance* child) {
    auto it = children_.find(child);
    DCHECK(it != children_.end());
    children_.erase(it);
    child->parent_ = nullptr;
  }

  bool ConnectToService(std::unique_ptr<ConnectParams>* connect_params) {
    if (!service_.is_bound())
      return false;

    std::unique_ptr<ConnectParams> params(std::move(*connect_params));
    if (!params->connect_callback().is_null()) {
        params->connect_callback().Run(mojom::ConnectResult::SUCCEEDED,
                                       identity_.user_id(), id_);
    }
    uint32_t source_id = mojom::kInvalidInstanceID;
    CapabilityRequest request;
    request.interfaces.insert("*");
    Instance* source = service_manager_->GetExistingInstance(params->source());
    if (source) {
      request = GenerateCapabilityRequestForConnection(
          source->capability_spec_, identity_, capability_spec_);
      source_id = source->id();
    }

    // The target has specified that sources must request one of its provided
    // classes instead of specifying a wild-card for interfaces.
    if (HasClass(capability_spec_, kCapabilityClass_ExplicitClass) &&
        (request.interfaces.count("*") != 0)) {
      request.interfaces.erase("*");
    }

    service_->OnConnect(
        mojom::Identity::From(params->source()), source_id,
        params->TakeRemoteInterfaces(), params->TakeLocalInterfaces(),
        mojom::CapabilityRequest::From(request), params->target().name());

    return true;
  }

  void StartWithService(mojom::ServicePtr service) {
    CHECK(!service_);
    service_ = std::move(service);
    service_.set_connection_error_handler(
        base::Bind(&Instance::OnServiceLost, base::Unretained(this),
                   service_manager_->GetWeakPtr()));
    service_->OnStart(mojom::Identity::From(identity_), id_,
                      base::Bind(&Instance::OnInitializeResponse,
                                 base::Unretained(this)));
  }

  void StartWithClientProcessConnection(
      mojom::ClientProcessConnectionPtr client_process_connection) {
    mojom::ServicePtr service;
    service.Bind(mojom::ServicePtrInfo(
        std::move(client_process_connection->service), 0));
    pid_receiver_binding_.Bind(
        std::move(client_process_connection->pid_receiver_request));
    StartWithService(std::move(service));
  }

  void StartWithFilePath(const base::FilePath& path) {
    CHECK(!service_);
    runner_ = service_manager_->native_runner_factory_->Create(path);
    bool start_sandboxed = false;
    mojom::ServicePtr service = runner_->Start(
        path, identity_, start_sandboxed,
        base::Bind(&Instance::PIDAvailable, weak_factory_.GetWeakPtr()),
        base::Bind(&Instance::OnRunnerCompleted, weak_factory_.GetWeakPtr()));
    StartWithService(std::move(service));
  }

  mojom::ServiceInfoPtr CreateServiceInfo() const {
    mojom::ServiceInfoPtr info(mojom::ServiceInfo::New());
    info->id = id_;
    info->identity = mojom::Identity::From(identity_);
    info->pid = pid_;
    return info;
  }

  const CapabilitySpec& capability_spec() const {
    return capability_spec_;
  }
  const Identity& identity() const { return identity_; }
  uint32_t id() const { return id_; }

  // Service:
  bool OnConnect(Connection* connection) override {
    connection->AddInterface<mojom::ServiceManager>(this);
    return true;
  }

 private:
  // mojom::Connector implementation:
  void Connect(mojom::IdentityPtr target_ptr,
               mojom::InterfaceProviderRequest remote_interfaces,
               mojom::InterfaceProviderPtr local_interfaces,
               mojom::ClientProcessConnectionPtr client_process_connection,
               const ConnectCallback& callback) override {
    Identity target = target_ptr.To<Identity>();
    if (target.user_id() == mojom::kInheritUserID)
      target.set_user_id(identity_.user_id());

    if (!ValidateIdentity(target, callback))
      return;
    if (!ValidateClientProcessConnection(&client_process_connection, target,
                                         callback)) {
      return;
    }
    if (!ValidateCapabilities(target, callback))
      return;

    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);
    params->set_remote_interfaces(std::move(remote_interfaces));
    params->set_local_interfaces(std::move(local_interfaces));
    params->set_client_process_connection(std::move(client_process_connection));
    params->set_connect_callback(callback);
    service_manager_->Connect(std::move(params));
  }

  void Clone(mojom::ConnectorRequest request) override {
    connectors_.AddBinding(this, std::move(request));
  }

  // mojom::PIDReceiver:
  void SetPID(uint32_t pid) override {
    PIDAvailable(pid);
  }

  // InterfaceFactory<mojom::ServiceManager>:
  void Create(Connection* connection,
              mojom::ServiceManagerRequest request) override {
    service_manager_bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ServiceManager implementation:
  void AddListener(mojom::ServiceManagerListenerPtr listener) override {
    // TODO(beng): this should only track the instances matching this user, and
    // root.
    service_manager_->AddListener(std::move(listener));
  }

  bool ValidateIdentity(const Identity& identity,
                        const ConnectCallback& callback) {
    if (!IsValidName(identity.name())) {
      LOG(ERROR) << "Error: invalid Name: " << identity.name();
      callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                   mojom::kInheritUserID, mojom::kInvalidInstanceID);
      return false;
    }
    if (!base::IsValidGUID(identity.user_id())) {
      LOG(ERROR) << "Error: invalid user_id: " << identity.user_id();
      callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                   mojom::kInheritUserID, mojom::kInvalidInstanceID);
      return false;
    }
    return true;
  }

  bool ValidateClientProcessConnection(
      mojom::ClientProcessConnectionPtr* client_process_connection,
      const Identity& target,
      const ConnectCallback& callback) {
    if (!client_process_connection->is_null()) {
      if (!HasClass(capability_spec_, kCapabilityClass_ClientProcess)) {
        LOG(ERROR) << "Instance: " << identity_.name() << " attempting "
                   << "to register an instance for a process it created for "
                   << "target: " << target.name() << " without the "
                   << "mojo:shell{client_process} capability class.";
        callback.Run(mojom::ConnectResult::ACCESS_DENIED,
                     mojom::kInheritUserID, mojom::kInvalidInstanceID);
        return false;
      }

      if (!(*client_process_connection)->service.is_valid() ||
          !(*client_process_connection)->pid_receiver_request.is_valid()) {
        LOG(ERROR) << "Must supply both service AND "
                   << "pid_receiver_request when sending "
                   << "client_process_connection.";
        callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                     mojom::kInheritUserID, mojom::kInvalidInstanceID);
        return false;
      }
      if (service_manager_->GetExistingInstance(target)) {
        LOG(ERROR) << "Cannot client process matching existing identity:"
                   << "Name: " << target.name() << " User: "
                   << target.user_id() << " Instance: " << target.instance();
        callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                     mojom::kInheritUserID, mojom::kInvalidInstanceID);
        return false;
      }
    }
    return true;
  }

  bool ValidateCapabilities(const Identity& target,
                            const ConnectCallback& callback) {
    // TODO(beng): Need to do the following additional policy validation of
    // whether this instance is allowed to connect using:
    // - a non-null client_process_connection.
    if (target.user_id() != identity_.user_id() &&
        target.user_id() != mojom::kRootUserID &&
        !HasClass(capability_spec_, kCapabilityClass_UserID)) {
      LOG(ERROR) << "Instance: " << identity_.name() << " running as: "
                  << identity_.user_id() << " attempting to connect to: "
                  << target.name() << " as: " << target.user_id() << " without "
                  << " the mojo:shell{user_id} capability class.";
      callback.Run(mojom::ConnectResult::ACCESS_DENIED,
                   mojom::kInheritUserID, mojom::kInvalidInstanceID);
      return false;
    }
    if (!target.instance().empty() &&
        target.instance() != GetNamePath(target.name()) &&
        !HasClass(capability_spec_, kCapabilityClass_InstanceName)) {
      LOG(ERROR) << "Instance: " << identity_.name() << " attempting to "
                  << "connect to " << target.name() << " using Instance name: "
                  << target.instance() << " without the "
                  << "mojo:shell{instance_name} capability class.";
      callback.Run(mojom::ConnectResult::ACCESS_DENIED,
                   mojom::kInheritUserID, mojom::kInvalidInstanceID);
      return false;

    }

    if (allow_any_application_ ||
        capability_spec_.required.find(target.name()) !=
            capability_spec_.required.end()) {
      return true;
    }
    LOG(ERROR) << "Capabilities prevented connection from: " <<
                  identity_.name() << " to: " << target.name();
    callback.Run(mojom::ConnectResult::ACCESS_DENIED,
                 mojom::kInheritUserID, mojom::kInvalidInstanceID);
    return false;
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
    service_manager_->NotifyPIDAvailable(id_, pid_);
  }

  void OnServiceLost(base::WeakPtr<shell::ServiceManager> service_manager) {
    service_.reset();
    OnConnectionLost(service_manager);
  }

  void OnConnectionLost(base::WeakPtr<shell::ServiceManager> service_manager) {
    // Any time a Connector is lost or we lose the Service connection, it
    // may have been the last pipe using this Instance. If so, clean up.
    if (service_manager && connectors_.empty() && !service_) {
      // Deletes |this|.
      service_manager->OnInstanceError(this);
    }
  }

  void OnInitializeResponse(mojom::ConnectorRequest connector_request) {
    if (connector_request.is_pending()) {
      connectors_.AddBinding(this, std::move(connector_request));
      connectors_.set_connection_error_handler(
          base::Bind(&Instance::OnConnectionLost, base::Unretained(this),
                     service_manager_->GetWeakPtr()));
    }
  }

  // Callback when NativeRunner completes.
  void OnRunnerCompleted() {
    if (!runner_.get())
      return;  // We're in the destructor.

    service_manager_->OnInstanceError(this);
  }

  shell::ServiceManager* const service_manager_;

  // An id that identifies this instance. Distinct from pid, as a single process
  // may vend multiple application instances, and this object may exist before a
  // process is launched.
  const uint32_t id_;
  const Identity identity_;
  const CapabilitySpec capability_spec_;
  const bool allow_any_application_;
  std::unique_ptr<NativeRunner> runner_;
  mojom::ServicePtr service_;
  mojo::Binding<mojom::PIDReceiver> pid_receiver_binding_;
  mojo::BindingSet<mojom::Connector> connectors_;
  mojo::BindingSet<mojom::ServiceManager> service_manager_bindings_;
  base::ProcessId pid_ = base::kNullProcessId;
  Instance* parent_ = nullptr;
  std::set<Instance*> children_;
  base::WeakPtrFactory<Instance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
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
    std::unique_ptr<NativeRunnerFactory> native_runner_factory,
    mojom::ServicePtr catalog)
    : native_runner_factory_(std::move(native_runner_factory)),
      weak_ptr_factory_(this) {
  mojom::ServicePtr service;
  mojom::ServiceRequest request = mojo::GetProxy(&service);
  Instance* instance = CreateInstance(
      Identity(), CreateServiceManagerIdentity(), GetPermissiveCapabilities());
  instance->StartWithService(std::move(service));
  singletons_.insert(kServiceManagerName);
  service_context_.reset(new ServiceContext(this, std::move(request)));

  if (catalog)
    InitCatalog(std::move(catalog));
}

ServiceManager::~ServiceManager() {
  TerminateServiceManagerConnections();
  // Terminate any remaining instances.
  while (!identity_to_instance_.empty())
    OnInstanceError(identity_to_instance_.begin()->second);
  identity_to_resolver_.clear();
}

void ServiceManager::SetInstanceQuitCallback(
    base::Callback<void(const Identity&)> callback) {
  instance_quit_callback_ = callback;
}

void ServiceManager::Connect(std::unique_ptr<ConnectParams> params) {
  Connect(std::move(params), nullptr);
}

mojom::ServiceRequest ServiceManager::StartEmbedderService(
    const std::string& name) {
  std::unique_ptr<ConnectParams> params(new ConnectParams);

  Identity embedder_identity(name, mojom::kRootUserID);
  params->set_source(embedder_identity);
  params->set_target(embedder_identity);

  mojom::ServicePtr service;
  mojom::ServiceRequest request = mojo::GetProxy(&service);
  Connect(std::move(params), std::move(service));

  return request;
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManager, Service implementation:

bool ServiceManager::OnConnect(Connection* connection) {
  // The only interface we expose is mojom::ServiceManager, and access to this
  // interface is brokered by a policy specific to each caller, managed by the
  // caller's instance. Here we look to see who's calling, and forward to the
  // caller's instance to continue.
  Instance* instance = nullptr;
  for (const auto& entry : identity_to_instance_) {
    if (entry.second->id() == connection->GetRemoteInstanceID()) {
      instance = entry.second;
      break;
    }
  }
  DCHECK(instance);
  return instance->OnConnect(connection);
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManager, private:

void ServiceManager::InitCatalog(mojom::ServicePtr catalog) {
  // TODO(beng): It'd be great to build this from the manifest, however there's
  //             a bit of a chicken-and-egg problem.
  CapabilitySpec spec;
  Interfaces interfaces;
  interfaces.insert("filesystem::mojom::Directory");
  spec.provided["app"] = interfaces;
  Instance* instance = CreateInstance(CreateServiceManagerIdentity(),
                                      CreateCatalogIdentity(),
                                      spec);
  singletons_.insert(kCatalogName);
  instance->StartWithService(std::move(catalog));
}

mojom::Resolver* ServiceManager::GetResolver(const Identity& identity) {
  auto iter = identity_to_resolver_.find(identity);
  if (iter != identity_to_resolver_.end())
    return iter->second.get();

  mojom::ResolverPtr resolver_ptr;
  ConnectToInterface(this, identity, CreateCatalogIdentity(), &resolver_ptr);
  mojom::Resolver* resolver = resolver_ptr.get();
  identity_to_resolver_[identity] = std::move(resolver_ptr);
  return resolver;
}

void ServiceManager::TerminateServiceManagerConnections() {
  Instance* instance = GetExistingInstance(CreateServiceManagerIdentity());
  DCHECK(instance);
  OnInstanceError(instance);
}

void ServiceManager::OnInstanceError(Instance* instance) {
  const Identity identity = instance->identity();
  // Remove the Service Manager.
  auto it = identity_to_instance_.find(identity);
  DCHECK(it != identity_to_instance_.end());
  int id = instance->id();
  identity_to_instance_.erase(it);
  listeners_.ForAllPtrs([this, id](mojom::ServiceManagerListener* listener) {
                          listener->OnServiceStopped(id);
                        });
  delete instance;
  if (!instance_quit_callback_.is_null())
    instance_quit_callback_.Run(identity);
}

void ServiceManager::Connect(std::unique_ptr<ConnectParams> params,
                             mojom::ServicePtr service) {
  TRACE_EVENT_INSTANT1("mojo_shell", "ServiceManager::Connect",
                       TRACE_EVENT_SCOPE_THREAD, "original_name",
                       params->target().name());
  DCHECK(IsValidName(params->target().name()));
  DCHECK(base::IsValidGUID(params->target().user_id()));
  DCHECK_NE(mojom::kInheritUserID, params->target().user_id());
  DCHECK(!service.is_bound() || !identity_to_instance_.count(params->target()));

  // Connect to an existing matching instance, if possible.
  if (!service.is_bound() && ConnectToExistingInstance(&params))
    return;

  // The catalog needs to see the source identity as that of the originating
  // app so it loads the correct store. Since the catalog is itself run as root
  // when this re-enters Connect() it'll be handled by
  // ConnectToExistingInstance().
  mojom::Resolver* resolver =
      GetResolver(Identity(kServiceManagerName, params->target().user_id()));

  std::string name = params->target().name();
  resolver->ResolveMojoName(
      name, base::Bind(&shell::ServiceManager::OnGotResolvedName,
                       weak_ptr_factory_.GetWeakPtr(), base::Passed(&params),
                       base::Passed(&service)));
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

void ServiceManager::NotifyPIDAvailable(uint32_t id, base::ProcessId pid) {
  listeners_.ForAllPtrs([id, pid](mojom::ServiceManagerListener* listener) {
                          listener->OnServiceStarted(id, pid);
                        });
}

bool ServiceManager::ConnectToExistingInstance(
    std::unique_ptr<ConnectParams>* params) {
  Instance* instance = GetExistingInstance((*params)->target());
  return !!instance && instance->ConnectToService(params);
}

ServiceManager::Instance* ServiceManager::CreateInstance(
    const Identity& source,
    const Identity& target,
    const CapabilitySpec& spec) {
  CHECK(target.user_id() != mojom::kInheritUserID);
  Instance* instance = new Instance(this, target, spec);
  DCHECK(identity_to_instance_.find(target) ==
         identity_to_instance_.end());
  Instance* source_instance = GetExistingInstance(source);
  if (source_instance)
    source_instance->AddChild(instance);
  identity_to_instance_[target] = instance;
  mojom::ServiceInfoPtr info = instance->CreateServiceInfo();
  listeners_.ForAllPtrs([&info](mojom::ServiceManagerListener* listener) {
    listener->OnServiceCreated(info.Clone());
  });
  return instance;
}

void ServiceManager::AddListener(mojom::ServiceManagerListenerPtr listener) {
  // TODO(beng): filter instances provided by those visible to this service.
  mojo::Array<mojom::ServiceInfoPtr> instances;
  for (auto& instance : identity_to_instance_)
    instances.push_back(instance.second->CreateServiceInfo());
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

  Identity source_identity(kServiceManagerName, mojom::kInheritUserID);
  mojom::ServiceFactoryPtr factory;
  ConnectToInterface(this, source_identity, service_factory_identity,
                     &factory);
  mojom::ServiceFactory* factory_interface = factory.get();
  factory.set_connection_error_handler(base::Bind(
      &shell::ServiceManager::OnServiceFactoryLost,
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
                                       mojom::ServicePtr service,
                                       mojom::ResolveResultPtr result) {
  std::string instance_name = params->target().instance();
  if (instance_name == GetNamePath(params->target().name()) &&
      result->qualifier != GetNamePath(result->resolved_name)) {
    instance_name = result->qualifier;
  }
  Identity target(params->target().name(), params->target().user_id(),
                  instance_name);
  params->set_target(target);

  // It's possible that when this manifest request was issued, another one was
  // already in-progress and completed by the time this one did, and so the
  // requested application may already be running.
  if (ConnectToExistingInstance(&params))
    return;

  Identity source = params->source();
  // |capabilities_ptr| can be null when there is no manifest, e.g. for URL
  // types not resolvable by the resolver.
  CapabilitySpec capabilities = GetPermissiveCapabilities();
  if (!result->capabilities.is_null())
    capabilities = result->capabilities.To<CapabilitySpec>();

  // Services that request "all_users" class from the Service Manager are
  // allowed to field connection requests from any user. They also run with a
  // synthetic user id generated here. The user id provided via Connect() is
  // ignored. Additionally services with the "all_users" class are not tied to
  // the lifetime of the service that started them, instead they are owned by
  // the Service Manager.
  Identity source_identity_for_creation;
  if (HasClass(capabilities, kCapabilityClass_AllUsers)) {
    singletons_.insert(target.name());
    target.set_user_id(base::GenerateGUID());
    source_identity_for_creation = CreateServiceManagerIdentity();
  } else {
    source_identity_for_creation = params->source();
  }

  mojom::ClientProcessConnectionPtr client_process_connection =
      params->TakeClientProcessConnection();
  Instance* instance = CreateInstance(source_identity_for_creation,
                                      target, capabilities);

  // Below are various paths through which a new Instance can be bound to a
  // Service proxy.
  if (service.is_bound()) {
    // If a ServicePtr was provided, there's no more work to do: someone
    // is already holding a corresponding ServiceRequest.
    instance->StartWithService(std::move(service));
  } else if (!client_process_connection.is_null()) {
    // Likewise if a ClientProcessConnection was given via Connect(), it
    // provides the Service proxy to use.
    instance->StartWithClientProcessConnection(
        std::move(client_process_connection));
  } else {
    // Otherwise we create a new Service pipe.
    mojom::ServiceRequest request = GetProxy(&service);
    CHECK(!result->package_path.empty() && !result->capabilities.is_null());

    if (target.name() != result->resolved_name) {
      instance->StartWithService(std::move(service));
      Identity factory(result->resolved_name, target.user_id(),
                       instance_name);
      CreateServiceWithFactory(factory, target.name(),
                                   std::move(request));
    } else {
      instance->StartWithFilePath(result->package_path);
    }
  }

  // Now that the instance has a Service, we can connect to it.
  bool connected = instance->ConnectToService(&params);
  DCHECK(connected);
}

base::WeakPtr<ServiceManager> ServiceManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace shell
