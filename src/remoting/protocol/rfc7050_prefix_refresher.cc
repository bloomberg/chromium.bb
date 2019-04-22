// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rfc7050_prefix_refresher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "remoting/protocol/rfc7050_ip_synthesizer.h"

namespace {
// Network changes will fire multiple notifications in a short period of time.
// This delay reduces the number of DNS queries being sent out.
constexpr base::TimeDelta kRefreshDelay =
    base::TimeDelta::FromMilliseconds(500);
}  // namespace

namespace remoting {
namespace protocol {

Rfc7050PrefixRefresher::Rfc7050PrefixRefresher()
    : ip_synthesizer_(Rfc7050IpSynthesizer::GetInstance()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(net::NetworkChangeNotifier::HasNetworkChangeNotifier());

  // The first update should be run immediately so that the IP synthesizer is
  // ready to use right after the constructor is run.
  OnUpdateDns64Prefix();

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  net::NetworkChangeNotifier::AddDNSObserver(this);
}

Rfc7050PrefixRefresher::~Rfc7050PrefixRefresher() {
  net::NetworkChangeNotifier::RemoveDNSObserver(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void Rfc7050PrefixRefresher::OnDNSChanged() {
  ScheduleUpdateDns64Prefix();
}

void Rfc7050PrefixRefresher::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  ScheduleUpdateDns64Prefix();
}

void Rfc7050PrefixRefresher::ScheduleUpdateDns64Prefix() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (update_dns64_prefix_scheduled_) {
    return;
  }
  update_dns64_prefix_scheduled_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Rfc7050PrefixRefresher::OnUpdateDns64Prefix,
                     weak_factory_.GetWeakPtr()),
      kRefreshDelay);
}

void Rfc7050PrefixRefresher::OnUpdateDns64Prefix() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  update_dns64_prefix_scheduled_ = false;
  ip_synthesizer_->UpdateDns64Prefix(base::DoNothing());
}

}  // namespace protocol
}  // namespace remoting
