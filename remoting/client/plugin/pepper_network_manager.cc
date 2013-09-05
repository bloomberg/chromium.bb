// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_network_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/private/network_list_private.h"
#include "remoting/client/plugin/pepper_util.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"

namespace remoting {

PepperNetworkManager::PepperNetworkManager(const pp::InstanceHandle& instance)
    : monitor_(instance, &PepperNetworkManager::OnNetworkListCallbackHandler,
               this),
      start_count_(0),
      network_list_received_(false),
      weak_factory_(this) {
}

PepperNetworkManager::~PepperNetworkManager() {
  DCHECK(!start_count_);
}

void PepperNetworkManager::StartUpdating() {
  if (network_list_received_) {
    // Post a task to avoid reentrancy.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PepperNetworkManager::SendNetworksChangedSignal,
                              weak_factory_.GetWeakPtr()));
  }
  ++start_count_;
}

void PepperNetworkManager::StopUpdating() {
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

// static
void PepperNetworkManager::OnNetworkListCallbackHandler(
    void* user_data,
    PP_Resource list_resource) {
  PepperNetworkManager* object = static_cast<PepperNetworkManager*>(user_data);
  pp::NetworkListPrivate list(pp::PASS_REF, list_resource);
  object->OnNetworkList(list);
}

void PepperNetworkManager::OnNetworkList(const pp::NetworkListPrivate& list) {
  network_list_received_ = true;

  std::vector<talk_base::Network*> networks;
  size_t count = list.GetCount();
  for (size_t i = 0; i < count; i++) {
    std::vector<pp::NetAddress> addresses;
    list.GetIpAddresses(i, &addresses);

    if (addresses.size() == 0)
      continue;

    talk_base::Network* network = new talk_base::Network(
        list.GetName(i), list.GetDisplayName(i), talk_base::IPAddress(), 0);

    for (size_t i = 0; i < addresses.size(); ++i) {
      talk_base::SocketAddress address;
      PpNetAddressToSocketAddress(addresses[i], &address);
      network->AddIP(address.ipaddr());
    }

    networks.push_back(network);
  }

  bool changed = false;
  MergeNetworkList(networks, &changed);
  if (changed)
    SignalNetworksChanged();
}

void PepperNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace remoting
