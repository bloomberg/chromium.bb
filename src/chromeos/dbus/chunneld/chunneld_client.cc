// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/chunneld/chunneld_client.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/chunneld/dbus-constants.h"

namespace chromeos {

class ChunneldClientImpl : public ChunneldClient {
 public:
  ChunneldClientImpl() {}

  ~ChunneldClientImpl() override = default;

  ChunneldClientImpl(const ChunneldClientImpl&) = delete;
  ChunneldClientImpl& operator=(const ChunneldClientImpl&) = delete;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    chunneld_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    chunneld_proxy_ = bus->GetObjectProxy(
        vm_tools::chunneld::kChunneldServiceName,
        dbus::ObjectPath(vm_tools::chunneld::kChunneldServicePath));
    chunneld_proxy_->SetNameOwnerChangedCallback(
        base::BindRepeating(&ChunneldClientImpl::NameOwnerChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  template <typename ResponseProto>
  void OnDBusProtoResponse(DBusMethodCallback<ResponseProto> callback,
                           dbus::Response* dbus_response) {
    if (!dbus_response) {
      std::move(callback).Run(absl::nullopt);
      return;
    }
    ResponseProto reponse_proto;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&reponse_proto)) {
      LOG(ERROR) << "Failed to parse proto from " << dbus_response->GetMember();
      std::move(callback).Run(absl::nullopt);
      return;
    }
    std::move(callback).Run(std::move(reponse_proto));
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    if (!old_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.ChunneldServiceStopped();
      }
    }
    if (!new_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.ChunneldServiceStarted();
      }
    }
  }

  base::ObserverList<Observer> observer_list_;

  dbus::ObjectProxy* chunneld_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChunneldClientImpl> weak_ptr_factory_{this};
};

ChunneldClient::ChunneldClient() = default;

ChunneldClient::~ChunneldClient() = default;

std::unique_ptr<ChunneldClient> ChunneldClient::Create() {
  return std::make_unique<ChunneldClientImpl>();
}

}  // namespace chromeos
