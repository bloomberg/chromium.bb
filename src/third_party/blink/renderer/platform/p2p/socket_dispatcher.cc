// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/p2p/network_list_observer.h"
#include "third_party/blink/renderer/platform/p2p/socket_client_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

P2PSocketDispatcher::P2PSocketDispatcher()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      network_list_observers_(
          new base::ObserverListThreadSafe<blink::NetworkListObserver>()) {}

P2PSocketDispatcher::~P2PSocketDispatcher() {}

void P2PSocketDispatcher::AddNetworkListObserver(
    blink::NetworkListObserver* network_list_observer) {
  network_list_observers_->AddObserver(network_list_observer);
  PostCrossThreadTask(
      *main_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&P2PSocketDispatcher::RequestNetworkEventsIfNecessary,
                          scoped_refptr<P2PSocketDispatcher>(this)));
}

void P2PSocketDispatcher::RemoveNetworkListObserver(
    blink::NetworkListObserver* network_list_observer) {
  network_list_observers_->RemoveObserver(network_list_observer);
}

network::mojom::blink::P2PSocketManager*
P2PSocketDispatcher::GetP2PSocketManager() {
  base::AutoLock lock(p2p_socket_manager_lock_);
  if (!p2p_socket_manager_) {
    mojo::PendingRemote<network::mojom::blink::P2PSocketManager>
        p2p_socket_manager;
    p2p_socket_manager_receiver_ =
        p2p_socket_manager.InitWithNewPipeAndPassReceiver();
    p2p_socket_manager_ =
        mojo::SharedRemote<network::mojom::blink::P2PSocketManager>(
            std::move(p2p_socket_manager));
    p2p_socket_manager_.set_disconnect_handler(
        WTF::Bind(&P2PSocketDispatcher::OnConnectionError,
                  WTF::Unretained(this)),
        main_task_runner_);
  }
  PostCrossThreadTask(
      *main_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&P2PSocketDispatcher::RequestInterfaceIfNecessary,
                          scoped_refptr<P2PSocketDispatcher>(this)));
  return p2p_socket_manager_.get();
}

void P2PSocketDispatcher::NetworkListChanged(
    const Vector<net::NetworkInterface>& networks,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  networks_ = networks;
  default_ipv4_local_address_ = default_ipv4_local_address;
  default_ipv6_local_address_ = default_ipv6_local_address;

  // TODO(crbug.com/787254): Remove this helper when network_list_observer.h
  // gets moved from blink/public to blink/renderer, and operate over
  // WTF::Vector.
  std::vector<net::NetworkInterface> copy(networks.size());
  for (size_t i = 0; i < networks.size(); i++)
    copy[i] = networks[i];

  network_list_observers_->Notify(
      FROM_HERE, &blink::NetworkListObserver::OnNetworkListChanged,
      std::move(copy), default_ipv4_local_address, default_ipv6_local_address);
}

void P2PSocketDispatcher::RequestInterfaceIfNecessary() {
  if (!p2p_socket_manager_receiver_.is_valid())
    return;

  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      std::move(p2p_socket_manager_receiver_));
}

void P2PSocketDispatcher::RequestNetworkEventsIfNecessary() {
  if (network_notification_client_receiver_.is_bound()) {
    // TODO(crbug.com/787254): Remove this helper when network_list_observer.h
    // gets moved from blink/public to blink/renderer, and operate over
    // WTF::Vector.
    std::vector<net::NetworkInterface> copy(networks_.size());
    for (size_t i = 0; i < networks_.size(); i++)
      copy[i] = networks_[i];

    network_list_observers_->Notify(
        FROM_HERE, &blink::NetworkListObserver::OnNetworkListChanged,
        std::move(copy), default_ipv4_local_address_,
        default_ipv6_local_address_);
  } else {
    GetP2PSocketManager()->StartNetworkNotifications(
        network_notification_client_receiver_.BindNewPipeAndPassRemote());
  }
}

void P2PSocketDispatcher::OnConnectionError() {
  base::AutoLock lock(p2p_socket_manager_lock_);
  p2p_socket_manager_.reset();
}

}  // namespace blink
