// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_PEER_CONNECTION_OBSERVER_IMPL_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_PEER_CONNECTION_OBSERVER_IMPL_H_

#include "core/internal/mediums/webrtc/local_ice_candidate_listener.h"
#include "platform/public/single_thread_executor.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class ConnectionFlow;

class PeerConnectionObserverImpl : public webrtc::PeerConnectionObserver {
 public:
  PeerConnectionObserverImpl(
      ConnectionFlow* connection_flow,
      LocalIceCandidateListener local_ice_candidate_listener);
  ~PeerConnectionObserverImpl() override;

  // webrtc::PeerConnectionObserver:
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnRenegotiationNeeded() override;

  void DisconnectConnectionFlow() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void OffloadFromSignalingThread(Runnable runnable);

  // NOTE: This must be a recursive mutex due to the call interactions.
  RecursiveMutex mutex_;  // protects access to connection_flow_
  ConnectionFlow* connection_flow_ ABSL_GUARDED_BY(mutex_);
  LocalIceCandidateListener local_ice_candidate_listener_;
  SingleThreadExecutor single_threaded_signaling_offloader_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_PEER_CONNECTION_OBSERVER_IMPL_H_
