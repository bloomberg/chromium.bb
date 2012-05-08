// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_network_manager.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/network_list_private.h"
#include "ppapi/cpp/private/net_address_private.h"

namespace remoting {

PepperNetworkManager::PepperNetworkManager(const pp::InstanceHandle& instance)
    : monitor_(instance, &PepperNetworkManager::OnNetworkListCallbackHandler,
               this),
      start_count_(0),
      network_list_received_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

PepperNetworkManager::~PepperNetworkManager() {
  DCHECK(!start_count_);
}

void PepperNetworkManager::StartUpdating() {
  if (network_list_received_) {
    // Post a task to avoid reentrancy.
    MessageLoop::current()->PostTask(
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
    std::vector<PP_NetAddress_Private> addresses;
    list.GetIpAddresses(i, &addresses);

    if (addresses.size() == 0)
      continue;

    char address_bytes[sizeof(in6_addr)];
    if (!pp::NetAddressPrivate::GetAddress(
            addresses[0], &address_bytes, sizeof(address_bytes))) {
      LOG(ERROR) << "Failed to get address for network interface.";
      continue;
    }

    int prefix_length;

    // TODO(sergeyu): Copy all addresses, not only the first one.
    talk_base::IPAddress address;
    switch (pp::NetAddressPrivate::GetFamily(addresses[0])) {
      case PP_NETADDRESSFAMILY_IPV4: {
        in_addr* address_ipv4 = reinterpret_cast<in_addr*>(address_bytes);
        address = talk_base::IPAddress(*address_ipv4);
        prefix_length = sizeof(in_addr) * 8;
        break;
      }

      case PP_NETADDRESSFAMILY_IPV6: {
        in6_addr* address_ipv6 = reinterpret_cast<in6_addr*>(address_bytes);
        address = talk_base::IPAddress(*address_ipv6);
        prefix_length = sizeof(in6_addr) * 8;
        break;
      }

      default:
        LOG(WARNING) << "Skipping address with unknown family: "
                     << pp::NetAddressPrivate::GetFamily(addresses[0]);
        continue;
    }

    talk_base::Network* network = new talk_base::Network(
        list.GetName(i), list.GetDisplayName(i), address, prefix_length);
    network->AddIP(address);
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
