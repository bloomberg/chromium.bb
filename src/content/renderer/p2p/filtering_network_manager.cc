// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/filtering_network_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_permission.h"

namespace content {

FilteringNetworkManager::FilteringNetworkManager(
    rtc::NetworkManager* network_manager,
    const GURL& requesting_origin,
    media::MediaPermission* media_permission)
    : network_manager_(network_manager),
      media_permission_(media_permission),
      requesting_origin_(requesting_origin),
      weak_ptr_factory_(this) {
  thread_checker_.DetachFromThread();
  set_enumeration_permission(ENUMERATION_BLOCKED);

  // If the feature is not enabled, just return ALLOWED as it's requested.
  if (!media_permission_) {
    started_permission_check_ = true;
    set_enumeration_permission(ENUMERATION_ALLOWED);
    VLOG(3) << "media_permission is not passed, granting permission";
    return;
  }
}

FilteringNetworkManager::~FilteringNetworkManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This helps to catch the case if permission never comes back.
  if (!start_updating_time_.is_null())
    ReportMetrics(false);
}

base::WeakPtr<FilteringNetworkManager> FilteringNetworkManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FilteringNetworkManager::Initialize() {
  rtc::NetworkManagerBase::Initialize();
  if (media_permission_)
    CheckPermission();
}

void FilteringNetworkManager::StartUpdating() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(started_permission_check_);

  if (start_updating_time_.is_null()) {
    start_updating_time_ = base::TimeTicks::Now();
    network_manager_->SignalNetworksChanged.connect(
        this, &FilteringNetworkManager::OnNetworksChanged);
  }

  // Update |pending_network_update_| and |start_count_| before calling
  // StartUpdating, in case the update signal is fired synchronously.
  pending_network_update_ = true;
  ++start_count_;
  network_manager_->StartUpdating();
  // If we have not sent the first update, which implies we have not received
  // the first network update from the base network manager, we wait until the
  // base network manager signals a network change for us to populate the
  // network information in |OnNetworksChanged| and fire the event there.
  if (sent_first_update_) {
    FireEventIfStarted();
  }
}

void FilteringNetworkManager::StopUpdating() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_manager_->StopUpdating();
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

void FilteringNetworkManager::GetNetworks(NetworkList* networks) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  networks->clear();

  if (enumeration_permission() == ENUMERATION_ALLOWED)
    NetworkManagerBase::GetNetworks(networks);

  VLOG(3) << "GetNetworks() returns " << networks->size() << " networks.";
}

webrtc::MdnsResponderInterface* FilteringNetworkManager::GetMdnsResponder()
    const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (enumeration_permission() == ENUMERATION_ALLOWED)
    return nullptr;

  return network_manager_->GetMdnsResponder();
}

void FilteringNetworkManager::CheckPermission() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!started_permission_check_);

  started_permission_check_ = true;
  pending_permission_checks_ = 2;

  VLOG(1) << "FilteringNetworkManager checking permission status.";
  // Request for media permission asynchronously.
  media_permission_->HasPermission(
      media::MediaPermission::AUDIO_CAPTURE,
      base::BindOnce(&FilteringNetworkManager::OnPermissionStatus,
                     GetWeakPtr()));
  media_permission_->HasPermission(
      media::MediaPermission::VIDEO_CAPTURE,
      base::BindOnce(&FilteringNetworkManager::OnPermissionStatus,
                     GetWeakPtr()));
}

void FilteringNetworkManager::OnPermissionStatus(bool granted) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(pending_permission_checks_, 0);
  VLOG(1) << "FilteringNetworkManager received permission status: "
          << (granted ? "granted" : "denied");
  IPPermissionStatus old_status = GetIPPermissionStatus();

  --pending_permission_checks_;

  if (granted)
    set_enumeration_permission(ENUMERATION_ALLOWED);

  // If the IP permission status changed *and* we have an up-to-date network
  // list, fire a network change event.
  if (GetIPPermissionStatus() != old_status && !pending_network_update_)
    FireEventIfStarted();
}

void FilteringNetworkManager::OnNetworksChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_network_update_ = false;

  // Update the default local addresses.
  rtc::IPAddress ipv4_default;
  rtc::IPAddress ipv6_default;
  network_manager_->GetDefaultLocalAddress(AF_INET, &ipv4_default);
  network_manager_->GetDefaultLocalAddress(AF_INET6, &ipv6_default);
  set_default_local_addresses(ipv4_default, ipv6_default);

  // Copy and merge the networks. Fire a signal if the permission status is
  // known.
  NetworkList networks;
  network_manager_->GetNetworks(&networks);
  NetworkList copied_networks;
  copied_networks.reserve(networks.size());
  for (rtc::Network* network : networks) {
    auto copied_network = std::make_unique<rtc::Network>(*network);
    copied_network->set_default_local_address_provider(this);
    copied_network->set_mdns_responder_provider(this);
    copied_networks.push_back(copied_network.release());
  }
  bool changed;
  MergeNetworkList(copied_networks, &changed);
  // We wait until our permission status is known before firing a network
  // change signal, so that the listener(s) don't miss out on receiving a
  // full network list.
  if (changed && GetIPPermissionStatus() != PERMISSION_UNKNOWN)
    FireEventIfStarted();
}

void FilteringNetworkManager::ReportMetrics(bool report_start_latency) {
  if (report_start_latency) {
    ReportTimeToUpdateNetworkList(base::TimeTicks::Now() -
                                  start_updating_time_);
  }

  ReportIPPermissionStatus(GetIPPermissionStatus());
}

IPPermissionStatus FilteringNetworkManager::GetIPPermissionStatus() const {
  if (enumeration_permission() == ENUMERATION_ALLOWED) {
    return media_permission_ ? PERMISSION_GRANTED_WITH_CHECKING
                             : PERMISSION_GRANTED_WITHOUT_CHECKING;
  }

  if (!pending_permission_checks_ &&
      enumeration_permission() == ENUMERATION_BLOCKED) {
    return PERMISSION_DENIED;
  }

  return PERMISSION_UNKNOWN;
}

void FilteringNetworkManager::FireEventIfStarted() {
  if (!start_count_)
    return;

  if (!sent_first_update_)
    ReportMetrics(true);

  // Post a task to avoid reentrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FilteringNetworkManager::SendNetworksChangedSignal,
                     GetWeakPtr()));

  sent_first_update_ = true;
}

void FilteringNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace content
