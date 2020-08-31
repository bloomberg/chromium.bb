// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/shill_service_client.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill/fake_shill_service_client.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

#ifndef DBUS_ERROR_UNKNOWN_OBJECT
// The linux_chromeos ASAN builder has an older version of dbus-protocol.h
// so make sure this is defined.
#define DBUS_ERROR_UNKNOWN_OBJECT "org.freedesktop.DBus.Error.UnknownObject"
#endif

ShillServiceClient* g_instance = nullptr;

// Error callback for GetProperties.
void OnGetDictionaryError(const std::string& method_name,
                          const dbus::ObjectPath& service_path,
                          ShillServiceClient::DictionaryValueCallback callback,
                          const std::string& error_name,
                          const std::string& error_message) {
  const std::string log_string = "Failed to call org.chromium.shill.Service." +
                                 method_name + " for: " + service_path.value() +
                                 ": " + error_name + ": " + error_message;

  // Suppress ERROR messages for UnknownMethod/Object" since this can
  // happen under normal conditions. See crbug.com/130660 and crbug.com/222210.
  if (error_name == DBUS_ERROR_UNKNOWN_METHOD ||
      error_name == DBUS_ERROR_UNKNOWN_OBJECT)
    VLOG(1) << log_string;
  else
    LOG(ERROR) << log_string;

  base::DictionaryValue empty_dictionary;
  std::move(callback).Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
}

// The ShillServiceClient implementation.
class ShillServiceClientImpl : public ShillServiceClient {
 public:
  explicit ShillServiceClientImpl(dbus::Bus* bus) : bus_(bus) {}

  ~ShillServiceClientImpl() override {
    for (HelperMap::iterator iter = helpers_.begin(); iter != helpers_.end();
         ++iter) {
      ShillClientHelper* helper = iter->second;
      bus_->RemoveObjectProxy(shill::kFlimflamServiceName,
                              helper->object_proxy()->object_path(),
                              base::DoNothing());
      delete helper;
    }
  }

  void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(service_path)->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(service_path)->RemovePropertyChangedObserver(observer);
  }

  void GetProperties(const dbus::ObjectPath& service_path,
                     DictionaryValueCallback callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetPropertiesFunction);
    auto callback_adapted =
        base::AdaptCallbackForRepeating(std::move(callback));
    GetHelper(service_path)
        ->CallDictionaryValueMethodWithErrorCallback(
            &method_call,
            base::BindOnce(callback_adapted, DBUS_METHOD_CALL_SUCCESS),
            base::BindOnce(&OnGetDictionaryError, "GetProperties", service_path,
                           callback_adapted));
  }

  void SetProperty(const dbus::ObjectPath& service_path,
                   const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void SetProperties(const dbus::ObjectPath& service_path,
                     const base::DictionaryValue& properties,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kSetPropertiesFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void ClearProperty(const dbus::ObjectPath& service_path,
                     const std::string& name,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void ClearProperties(const dbus::ObjectPath& service_path,
                       const std::vector<std::string>& names,
                       ListValueCallback callback,
                       ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kClearPropertiesFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(names);
    GetHelper(service_path)
        ->CallListValueMethodWithErrorCallback(
            &method_call, std::move(callback), std::move(error_callback));
  }

  void Connect(const dbus::ObjectPath& service_path,
               base::OnceClosure callback,
               ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kConnectFunction);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void Disconnect(const dbus::ObjectPath& service_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kDisconnectFunction);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void Remove(const dbus::ObjectPath& service_path,
              base::OnceClosure callback,
              ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kRemoveServiceFunction);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void ActivateCellularModem(const dbus::ObjectPath& service_path,
                             const std::string& carrier,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kActivateCellularModemFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(carrier);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void CompleteCellularActivation(const dbus::ObjectPath& service_path,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kCompleteCellularActivationFunction);
    dbus::MessageWriter writer(&method_call);
    GetHelper(service_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void GetLoadableProfileEntries(const dbus::ObjectPath& service_path,
                                 DictionaryValueCallback callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetLoadableProfileEntriesFunction);
    auto callback_adapted =
        base::AdaptCallbackForRepeating(std::move(callback));
    GetHelper(service_path)
        ->CallDictionaryValueMethodWithErrorCallback(
            &method_call,
            base::BindOnce(callback_adapted, DBUS_METHOD_CALL_SUCCESS),
            base::BindOnce(&OnGetDictionaryError, "GetLoadableProfileEntries",
                           service_path, callback_adapted));
  }

  void GetWiFiPassphrase(const dbus::ObjectPath& service_path,
                         StringCallback callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetWiFiPassphraseFunction);

    GetHelper(service_path)
        ->CallStringMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  ShillServiceClient::TestInterface* GetTestInterface() override {
    return nullptr;
  }

 private:
  typedef std::map<std::string, ShillClientHelper*> HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& service_path) {
    HelperMap::iterator it = helpers_.find(service_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(shill::kFlimflamServiceName, service_path);
    ShillClientHelper* helper = new ShillClientHelper(object_proxy);
    helper->SetReleasedCallback(
        base::BindOnce(&ShillServiceClientImpl::NotifyReleased,
                       weak_ptr_factory_.GetWeakPtr()));
    helper->MonitorPropertyChanged(shill::kFlimflamServiceInterface);
    helpers_.insert(HelperMap::value_type(service_path.value(), helper));
    return helper;
  }

  void NotifyReleased(ShillClientHelper* helper) {
    // New Shill Service DBus objects are created relatively frequently, so
    // remove them when they become inactive (no observers and no active method
    // calls).
    dbus::ObjectPath object_path = helper->object_proxy()->object_path();
    // Make sure we don't release the proxy used by ShillManagerClient ("/").
    // This shouldn't ever happen, but might if a bug in the code requests
    // a service with path "/", or a bug in Shill passes "/" as a service path.
    // Either way this would cause an invalid memory access in
    // ShillManagerClient, see crbug.com/324849.
    if (object_path == dbus::ObjectPath(shill::kFlimflamServicePath)) {
      NET_LOG(ERROR) << "ShillServiceClient service has invalid path: "
                     << shill::kFlimflamServicePath;
      return;
    }
    bus_->RemoveObjectProxy(shill::kFlimflamServiceName, object_path,
                            base::DoNothing());
    helpers_.erase(object_path.value());
    delete helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  base::WeakPtrFactory<ShillServiceClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShillServiceClientImpl);
};

}  // namespace

ShillServiceClient::ShillServiceClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShillServiceClient::~ShillServiceClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ShillServiceClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ShillServiceClientImpl(bus);
}

// static
void ShillServiceClient::InitializeFake() {
  new FakeShillServiceClient();
}

// static
void ShillServiceClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ShillServiceClient* ShillServiceClient::Get() {
  return g_instance;
}

}  // namespace chromeos
