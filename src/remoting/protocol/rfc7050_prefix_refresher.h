// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RFC7050_PREFIX_REFRESHER_H_
#define REMOTING_PROTOCOL_RFC7050_PREFIX_REFRESHER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "net/base/network_change_notifier.h"

namespace remoting {
namespace protocol {

class Rfc7050IpSynthesizer;

// Helper class to observe network changes and update Rfc7050IpSynthesizer's
// DNS64 prefix when the network is changed.
class Rfc7050PrefixRefresher
    : public net::NetworkChangeNotifier::DNSObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  Rfc7050PrefixRefresher();
  ~Rfc7050PrefixRefresher() override;

 private:
  // DNSObserver override.
  void OnDNSChanged() override;

  // NetworkChangeObserver override.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  void ScheduleUpdateDns64Prefix();
  void OnUpdateDns64Prefix();

  bool update_dns64_prefix_scheduled_ = false;
  Rfc7050IpSynthesizer* ip_synthesizer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<Rfc7050PrefixRefresher> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Rfc7050PrefixRefresher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RFC7050_PREFIX_REFRESHER_H_
