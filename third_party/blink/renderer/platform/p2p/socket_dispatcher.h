// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// P2PSocketDispatcher is a per-renderer object that dispatchers all
// P2P messages received from the browser and relays all P2P messages
// sent to the browser. P2PSocketClient instances register themselves
// with the dispatcher using RegisterClient() and UnregisterClient().
//
// Relationship of classes.
//
//       P2PSocketHost                     P2PSocketClient
//            ^                                   ^
//            |                                   |
//            v                  IPC              v
//  P2PSocketDispatcherHost  <--------->  P2PSocketDispatcher
//
// P2PSocketDispatcher receives and dispatches messages on the
// IO thread.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_SOCKET_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_SOCKET_DISPATCHER_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom-blink.h"
#include "third_party/blink/renderer/platform/p2p/network_list_manager.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {
class NetworkListObserver;
}

namespace blink {

// This class is created on the main thread, but is used primarily on the
// WebRTC worker threads.
class PLATFORM_EXPORT P2PSocketDispatcher
    : public base::RefCountedThreadSafe<P2PSocketDispatcher>,
      public blink::NetworkListManager,
      public network::mojom::blink::P2PNetworkNotificationClient {
 public:
  P2PSocketDispatcher();

  // blink::NetworkListManager interface:
  void AddNetworkListObserver(
      blink::NetworkListObserver* network_list_observer) override;
  void RemoveNetworkListObserver(
      blink::NetworkListObserver* network_list_observer) override;

  network::mojom::blink::P2PSocketManager* GetP2PSocketManager();

 private:
  friend class base::RefCountedThreadSafe<P2PSocketDispatcher>;

  ~P2PSocketDispatcher() override;

  // network::mojom::blink::P2PNetworkNotificationClient interface.
  void NetworkListChanged(
      const Vector<net::NetworkInterface>& networks,
      const net::IPAddress& default_ipv4_local_address,
      const net::IPAddress& default_ipv6_local_address) override;

  void RequestInterfaceIfNecessary();
  void RequestNetworkEventsIfNecessary();

  void OnConnectionError();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // TODO(crbug.com/787254): When moving NetworkListObserver to Oilpan,
  // thread-safety needs to be taken into account.
  scoped_refptr<base::ObserverListThreadSafe<blink::NetworkListObserver>>
      network_list_observers_;

  mojo::PendingReceiver<network::mojom::blink::P2PSocketManager>
      p2p_socket_manager_receiver_;
  mojo::SharedRemote<network::mojom::blink::P2PSocketManager>
      p2p_socket_manager_;
  base::Lock p2p_socket_manager_lock_;

  // Cached from last |NetworkListChanged| call.
  Vector<net::NetworkInterface> networks_;
  net::IPAddress default_ipv4_local_address_;
  net::IPAddress default_ipv6_local_address_;

  mojo::Receiver<network::mojom::blink::P2PNetworkNotificationClient>
      network_notification_client_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_SOCKET_DISPATCHER_H_
