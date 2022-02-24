// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_DEPENDENCY_FACTORY_H_

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_video_perf_reporter.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc_overrides/metronome_provider.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace base {
class WaitableEvent;
}

namespace cricket {
class PortAllocator;
}

namespace media {
class DecoderFactory;
class GpuVideoAcceleratorFactories;
}

namespace rtc {
class Thread;
}

namespace gfx {
class ColorSpace;
}

namespace blink {

class IpcNetworkManager;
class IpcPacketSocketFactory;
class MdnsResponderAdapter;
class P2PSocketDispatcher;
class RTCPeerConnectionHandlerClient;
class RTCPeerConnectionHandler;
class WebLocalFrame;
class WebRtcAudioDeviceImpl;

// Object factory for RTC PeerConnections.
class MODULES_EXPORT PeerConnectionDependencyFactory
    : public GarbageCollected<PeerConnectionDependencyFactory>,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleObserver {
  USING_PRE_FINALIZER(PeerConnectionDependencyFactory,
                      CleanupPeerConnectionFactory);

 public:
  static const char kSupplementName[];

  static PeerConnectionDependencyFactory& From(ExecutionContext& context);
  PeerConnectionDependencyFactory(
      ExecutionContext& context,
      base::PassKey<PeerConnectionDependencyFactory>);

  PeerConnectionDependencyFactory(const PeerConnectionDependencyFactory&) =
      delete;
  PeerConnectionDependencyFactory& operator=(
      const PeerConnectionDependencyFactory&) = delete;

  ~PeerConnectionDependencyFactory() override;

  // Create a RTCPeerConnectionHandler object.
  std::unique_ptr<RTCPeerConnectionHandler> CreateRTCPeerConnectionHandler(
      RTCPeerConnectionHandlerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool force_encoded_audio_insertable_streams,
      bool force_encoded_video_insertable_streams);

  // Create a proxy object for a VideoTrackSource that makes sure it's called on
  // the correct threads.
  virtual scoped_refptr<webrtc::VideoTrackSourceInterface>
  CreateVideoTrackSourceProxy(webrtc::VideoTrackSourceInterface* source);

  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual scoped_refptr<webrtc::MediaStreamInterface> CreateLocalMediaStream(
      const String& label);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack(
      const String& id,
      webrtc::VideoTrackSourceInterface* source);

  // Asks the libjingle PeerConnection factory to create a libjingle
  // PeerConnection object.
  // The PeerConnection object is owned by PeerConnectionHandler.
  virtual scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      blink::WebLocalFrame* web_frame,
      webrtc::PeerConnectionObserver* observer,
      ExceptionState& exception_state);
  size_t open_peer_connections() const;
  void OnPeerConnectionClosed();
  scoped_refptr<MetronomeProvider> metronome_provider() const;

  // Creates a PortAllocator that uses Chrome IPC sockets and enforces privacy
  // controls according to the permissions granted on the page.
  virtual std::unique_ptr<cricket::PortAllocator> CreatePortAllocator(
      blink::WebLocalFrame* web_frame);

  // Creates an AsyncResolverFactory that uses the networking Mojo service.
  virtual std::unique_ptr<webrtc::AsyncResolverFactory>
  CreateAsyncResolverFactory();

  // Creates a libjingle representation of an ice candidate.
  virtual webrtc::IceCandidateInterface* CreateIceCandidate(
      const String& sdp_mid,
      int sdp_mline_index,
      const String& sdp);

  // Returns the most optimistic view of the capabilities of the system for
  // sending or receiving media of the given kind ("audio" or "video").
  virtual std::unique_ptr<webrtc::RtpCapabilities> GetSenderCapabilities(
      const String& kind);
  virtual std::unique_ptr<webrtc::RtpCapabilities> GetReceiverCapabilities(
      const String& kind);

  blink::WebRtcAudioDeviceImpl* GetWebRtcAudioDevice();

  void EnsureInitialized();

  // Returns the SingleThreadTaskRunner suitable for running WebRTC networking.
  // An rtc::Thread will have already been created.
  scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcNetworkTaskRunner();

  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetWebRtcSignalingTaskRunner();

  media::GpuVideoAcceleratorFactories* GetGpuFactories();

  void Trace(Visitor*) const override;

 protected:
  // Ctor for tests.
  PeerConnectionDependencyFactory();

  virtual const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
  GetPcFactory();
  virtual bool PeerConnectionFactoryCreated();

  // Helper method to create a WebRtcAudioDeviceImpl.
  void EnsureWebRtcAudioDeviceImpl();

  // Number of non-closed peer connections in existence.
  size_t open_peer_connections_ = 0u;

 private:
  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  // Functions related to Stun probing trial to determine how fast we could send
  // Stun request without being dropped by NAT.
  void TryScheduleStunProbeTrial();

  // Creates |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  void CreatePeerConnectionFactory();

  void InitializeSignalingThread(
      const gfx::ColorSpace& render_color_space,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      media::DecoderFactory* media_decoder_factory,
      base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnNetworkThread(
      base::WaitableEvent* event,
      std::unique_ptr<MdnsResponderAdapter> mdns_responder);
  static void DeleteIpcNetworkManager(
      std::unique_ptr<IpcNetworkManager> network_manager,
      base::WaitableEvent* event);
  void CleanupPeerConnectionFactory();

  // network_manager_ must be deleted on the network thread. The network manager
  // uses |p2p_socket_dispatcher_|.
  std::unique_ptr<IpcNetworkManager> network_manager_;
  std::unique_ptr<IpcPacketSocketFactory> socket_factory_;

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;
  scoped_refptr<MetronomeProvider> metronome_provider_;
  scoped_refptr<MetronomeSource> metronome_source_;

  // Dispatches all P2P sockets.
  Member<P2PSocketDispatcher> p2p_socket_dispatcher_;

  scoped_refptr<blink::WebRtcAudioDeviceImpl> audio_device_;

  media::GpuVideoAcceleratorFactories* gpu_factories_;

  WebrtcVideoPerfReporter webrtc_video_perf_reporter_;

  bool encode_decode_capabilities_reported_ = false;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
