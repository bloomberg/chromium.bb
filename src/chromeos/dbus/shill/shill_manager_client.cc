// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/shill_manager_client.h"

#include <ios>
#include <memory>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/values.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

ShillManagerClient* g_instance = nullptr;

// The ShillManagerClient implementation.
class ShillManagerClientImpl : public ShillManagerClient {
 public:
  ShillManagerClientImpl() = default;

  ShillManagerClientImpl(const ShillManagerClientImpl&) = delete;
  ShillManagerClientImpl& operator=(const ShillManagerClientImpl&) = delete;

  ~ShillManagerClientImpl() override = default;

  ////////////////////////////////////
  // ShillManagerClient overrides.
  void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override {
    helper_->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override {
    helper_->RemovePropertyChangedObserver(observer);
  }

  void GetProperties(DBusMethodCallback<base::Value> callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetPropertiesFunction);
    helper_->CallValueMethod(
        &method_call,
        base::BindOnce(&ShillClientHelper::OnGetProperties,
                       dbus::ObjectPath(shill::kFlimflamServicePath),
                       std::move(callback)));
  }

  void GetNetworksForGeolocation(
      DBusMethodCallback<base::Value> callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetNetworksForGeolocation);
    helper_->CallValueMethod(&method_call, std::move(callback));
  }

  void SetProperty(const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override {
    // This property is read-only and can only be mutated by the specialized
    // method exposed in DBus API.
    if (name == shill::kDNSProxyDOHProvidersProperty) {
      SetDNSProxyDOHProviders(value, std::move(callback),
                              std::move(error_callback));
      return;
    }

    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void RequestScan(const std::string& type,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kRequestScanFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void EnableTechnology(const std::string& type,
                        base::OnceClosure callback,
                        ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kEnableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void SetNetworkThrottlingStatus(const NetworkThrottlingStatus& status,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetNetworkThrottlingFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(status.enabled);
    writer.AppendUint32(status.upload_rate_kbits);
    writer.AppendUint32(status.download_rate_kbits);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void DisableTechnology(const std::string& type,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kDisableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void ConfigureService(const base::Value& properties,
                        ObjectPathCallback callback,
                        ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void ConfigureServiceForProfile(const dbus::ObjectPath& profile_path,
                                  const base::Value& properties,
                                  ObjectPathCallback callback,
                                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceForProfileFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(dbus::ObjectPath(profile_path));
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void GetService(const base::Value& properties,
                  ObjectPathCallback callback,
                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void ConnectToBestServices(base::OnceClosure callback,
                             ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConnectToBestServicesFunction);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void AddPasspointCredentials(const dbus::ObjectPath& profile_path,
                               const base::Value& properties,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kAddPasspointCredentialsFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(profile_path);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  TestInterface* GetTestInterface() override { return nullptr; }

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(shill::kFlimflamServiceName,
                                 dbus::ObjectPath(shill::kFlimflamServicePath));
    helper_ = std::make_unique<ShillClientHelper>(proxy_);
    helper_->MonitorPropertyChanged(shill::kFlimflamManagerInterface);
  }

 private:
  // Used by SetProperty call to reroute kDNSProxyDOHProviders to the underlying
  // specialized method in the DBus API.
  void SetDNSProxyDOHProviders(const base::Value& providers,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetDNSProxyDOHProvidersFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServiceProperties(&writer, providers);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  dbus::ObjectProxy* proxy_ = nullptr;
  std::unique_ptr<ShillClientHelper> helper_;
};

}  // namespace

ShillManagerClient::ShillManagerClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShillManagerClient::~ShillManagerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ShillManagerClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new ShillManagerClientImpl)->Init(bus);
}

// static
void ShillManagerClient::InitializeFake() {
  new FakeShillManagerClient();
}

// static
void ShillManagerClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ShillManagerClient* ShillManagerClient::Get() {
  return g_instance;
}

}  // namespace chromeos
