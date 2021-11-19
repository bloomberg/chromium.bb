// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/cpu.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/openssl_util.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_permission.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "net/net_buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/peerconnection/execution_context_metronome_provider.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/p2p/empty_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/filtering_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_socket_factory.h"
#include "third_party/blink/renderer/platform/p2p/mdns_responder_adapter.h"
#include "third_party/blink/renderer/platform/p2p/port_allocator.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/webrtc/api/call/call_factory_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/rtc_event_log/rtc_event_log_factory.h"
#include "third_party/webrtc/api/video_track_source_proxy_factory.h"
#include "third_party/webrtc/media/engine/fake_video_codec_factory.h"
#include "third_party/webrtc/media/engine/multiplex_codec_factory.h"
#include "third_party/webrtc/media/engine/webrtc_media_engine.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/rtc_base/openssl_stream_adapter.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "third_party/webrtc/rtc_base/ssl_adapter.h"
#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace WTF {
template <>
struct CrossThreadCopier<base::RepeatingCallback<void(base::TimeDelta)>>
    : public CrossThreadCopierPassThrough<
          base::RepeatingCallback<void(base::TimeDelta)>> {
  STATIC_ONLY(CrossThreadCopier);
};
}  // namespace WTF

namespace blink {

namespace {

using PassKey = base::PassKey<PeerConnectionDependencyFactory>;

enum WebRTCIPHandlingPolicy {
  DEFAULT,
  DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES,
  DEFAULT_PUBLIC_INTERFACE_ONLY,
  DISABLE_NON_PROXIED_UDP,
};

WebRTCIPHandlingPolicy GetWebRTCIPHandlingPolicy(const String& preference) {
  if (preference == kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces)
    return DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES;
  if (preference == kWebRTCIPHandlingDefaultPublicInterfaceOnly)
    return DEFAULT_PUBLIC_INTERFACE_ONLY;
  if (preference == kWebRTCIPHandlingDisableNonProxiedUdp)
    return DISABLE_NON_PROXIED_UDP;
  return DEFAULT;
}

bool IsValidPortRange(uint16_t min_port, uint16_t max_port) {
  DCHECK(min_port <= max_port);
  return min_port != 0 && max_port != 0;
}

// PeerConnectionDependencies wants to own the factory, so we provide a simple
// object that delegates calls to the IpcPacketSocketFactory.
// TODO(zstein): Move the creation logic from IpcPacketSocketFactory in to this
// class.
class ProxyAsyncResolverFactory final : public webrtc::AsyncResolverFactory {
 public:
  explicit ProxyAsyncResolverFactory(IpcPacketSocketFactory* ipc_psf)
      : ipc_psf_(ipc_psf) {
    DCHECK(ipc_psf);
  }

  rtc::AsyncResolverInterface* Create() override {
    return ipc_psf_->CreateAsyncResolver();
  }

 private:
  IpcPacketSocketFactory* ipc_psf_;
};

// Encapsulates process-wide static dependencies used by
// `PeerConnectionDependencyFactory`, namely the threads used by WebRTC. This
// avoids allocating multiple threads per factory instance, as they are
// "heavy-weight" and we don't want to create them per frame.
class PeerConnectionStaticDeps {
 public:
  PeerConnectionStaticDeps()
      : chrome_signaling_thread_("WebRTC_Signaling"),
        chrome_network_thread_("WebRTC_Network") {
    if (base::FeatureList::IsEnabled(features::kWebRtcDistinctWorkerThread)) {
      chrome_worker_thread_.emplace("WebRTC_Worker");
    }
  }

  void EnsureChromeThreadsStarted() {
    if (!chrome_signaling_thread_.IsRunning())
      chrome_signaling_thread_.Start();
    if (!chrome_network_thread_.IsRunning())
      chrome_network_thread_.Start();

    if (chrome_worker_thread_ && !chrome_worker_thread_->IsRunning())
      chrome_worker_thread_->Start();

    // To allow sending to the signaling/worker threads.
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  }

  base::WaitableEvent& InitializeWorkerThread() {
    // If there is no worker thread, the event was already created SIGNALED.
    if (!chrome_worker_thread_)
      return init_worker_event;

    if (!worker_thread_) {
      PostCrossThreadTask(
          *chrome_worker_thread_->task_runner(), FROM_HERE,
          CrossThreadBindOnce(
              &PeerConnectionStaticDeps::InitializeOnThread,
              CrossThreadUnretained(&worker_thread_),
              CrossThreadUnretained(&init_worker_event),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskLatencyWorker)),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskDurationWorker))));
    }
    return init_worker_event;
  }

  base::WaitableEvent& InitializeNetworkThread() {
    if (!network_thread_) {
      PostCrossThreadTask(
          *chrome_network_thread_.task_runner(), FROM_HERE,
          CrossThreadBindOnce(
              &PeerConnectionStaticDeps::InitializeOnThread,
              CrossThreadUnretained(&network_thread_),
              CrossThreadUnretained(&init_network_event),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskLatencyNetwork)),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskDurationNetwork))));
    }
    return init_network_event;
  }

  base::WaitableEvent& InitializeSignalingThread() {
    if (!signaling_thread_) {
      PostCrossThreadTask(
          *chrome_signaling_thread_.task_runner(), FROM_HERE,
          CrossThreadBindOnce(
              &PeerConnectionStaticDeps::InitializeOnThread,
              CrossThreadUnretained(&signaling_thread_),
              CrossThreadUnretained(&init_signaling_event),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskLatencySignaling)),
              ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                  PeerConnectionStaticDeps::LogTaskDurationSignaling))));
    }
    return init_signaling_event;
  }

  rtc::Thread* GetSignalingThread() { return signaling_thread_; }
  rtc::Thread* GetWorkerThread() { return worker_thread_; }
  rtc::Thread* GetNetworkThread() { return network_thread_; }
  base::Thread& GetChromeSignalingThread() { return chrome_signaling_thread_; }
  base::Thread* GetChromeWorkerThread() {
    return chrome_worker_thread_.has_value() ? &*chrome_worker_thread_
                                             : nullptr;
  }
  base::Thread& GetChromeNetworkThread() { return chrome_network_thread_; }

 private:
  static void LogTaskLatencyWorker(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Latency.Worker", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskDurationWorker(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Duration.Worker", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskLatencyNetwork(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Latency.Network", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskDurationNetwork(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Duration.Network", sample, base::Microseconds(1),
        base::Seconds(10), 50);
  }
  static void LogTaskLatencySignaling(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Latency.Signaling", sample,
        base::Microseconds(1), base::Seconds(10), 50);
  }
  static void LogTaskDurationSignaling(base::TimeDelta sample) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "WebRTC.PeerConnection.Duration.Signaling", sample,
        base::Microseconds(1), base::Seconds(10), 50);
  }

  static void InitializeOnThread(
      rtc::Thread** thread,
      base::WaitableEvent* event,
      base::RepeatingCallback<void(base::TimeDelta)> latency_callback,
      base::RepeatingCallback<void(base::TimeDelta)> duration_callback) {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
    jingle_glue::JingleThreadWrapper::current()
        ->SetLatencyAndTaskDurationCallbacks(std::move(latency_callback),
                                             std::move(duration_callback));
    if (!*thread) {
      *thread = jingle_glue::JingleThreadWrapper::current();
      event->Signal();
    }
  }

  // PeerConnection threads. signaling_thread_ is created from the "current"
  // (main) chrome thread.
  rtc::Thread* signaling_thread_ = nullptr;
  rtc::Thread* worker_thread_ = nullptr;
  rtc::Thread* network_thread_ = nullptr;
  base::Thread chrome_signaling_thread_;
  absl::optional<base::Thread> chrome_worker_thread_;
  base::Thread chrome_network_thread_;

  // WaitableEvents for observing thread initialization.
  base::WaitableEvent init_signaling_event{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
  base::WaitableEvent init_worker_event{
      base::WaitableEvent::ResetPolicy::MANUAL,
      // If we don't create a separate worker thread, start the event as already
      // signaled so it doesn't block anything.
      base::FeatureList::IsEnabled(features::kWebRtcDistinctWorkerThread)
          ? base::WaitableEvent::InitialState::NOT_SIGNALED
          : base::WaitableEvent::InitialState::SIGNALED};
  base::WaitableEvent init_network_event{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  THREAD_CHECKER(thread_checker_);
};

PeerConnectionStaticDeps& StaticDeps() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(PeerConnectionStaticDeps, instance, ());
  return instance;
}

rtc::Thread* GetSignalingThread() {
  return StaticDeps().GetSignalingThread();
}
rtc::Thread* GetWorkerThread() {
  return StaticDeps().GetWorkerThread();
}
rtc::Thread* GetNetworkThread() {
  return StaticDeps().GetNetworkThread();
}
base::Thread& GetChromeSignalingThread() {
  return StaticDeps().GetChromeSignalingThread();
}
base::Thread* GetChromeWorkerThread() {
  return StaticDeps().GetChromeWorkerThread();
}
base::Thread& GetChromeNetworkThread() {
  return StaticDeps().GetChromeNetworkThread();
}

std::string GetTierName() {
  int processors = base::SysInfo::NumberOfProcessors();
  if (processors <= 2) {
    base::CPU cpu;
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const re2::RE2, low_tier_regexp,
                                    ("Atom|Celeron|Pentium|AMD [AE][0-9]-"));
    if (re2::RE2::PartialMatch(cpu.cpu_brand(), low_tier_regexp)) {
      return "LOW";
    }
  }

  if (processors < 4) {
    return "MID";
  }
  return "HIGH_OR_ULTRA";
}

struct UmaVideoConfig {
  webrtc::SdpVideoFormat format;
  absl::optional<std::string> scalability_mode;
  int decoder_weight;
  int encoder_weight;
};

void ReportUmaEncodeDecodeCapabilities(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::DecoderFactory* media_decoder_factory) {
  const gfx::ColorSpace& render_color_space =
      Platform::Current()->GetRenderingColorSpace();
  scoped_refptr<base::SequencedTaskRunner> media_task_runner =
      Platform::Current()->MediaThreadTaskRunner();

  // Create encoder/decoder factories.
  std::unique_ptr<webrtc::VideoEncoderFactory> webrtc_encoder_factory =
      blink::CreateWebrtcVideoEncoderFactory(gpu_factories);
  std::unique_ptr<webrtc::VideoDecoderFactory> webrtc_decoder_factory =
      blink::CreateWebrtcVideoDecoderFactory(
          gpu_factories, media_decoder_factory, std::move(media_task_runner),
          render_color_space);
  if (webrtc_encoder_factory && webrtc_decoder_factory) {
    using Sdp = webrtc::SdpVideoFormat;
    // Query for encode/decode support for H264, VP8, VP9, VP9 k-SVC.
    UmaVideoConfig video_configs[] = {
        {Sdp{"H264", Sdp::Parameters{{"level-asymmetry-allowed", "1"},
                                     {"packetization-mode", "1"},
                                     {"profile-level-id", "42001f"}}},
         absl::nullopt, 1, 32},
        {Sdp{"H264", Sdp::Parameters{{"level-asymmetry-allowed", "1"},
                                     {"packetization-mode", "1"},
                                     {"profile-level-id", "4d0032"}}},
         absl::nullopt, 2, 64},
        {Sdp{"VP8"}, absl::nullopt, 4, 128},
        {Sdp{"VP9"}, absl::nullopt, 8, 256},
        {Sdp{"VP9"}, "L3T3_KEY", 16, 512}};
    int uma_metric = 0;
    for (const auto& config : video_configs) {
      uma_metric += config.decoder_weight *
                    webrtc_decoder_factory
                        ->QueryCodecSupport(config.format,
                                            config.scalability_mode.has_value())
                        .is_power_efficient;

      uma_metric +=
          config.encoder_weight *
          webrtc_encoder_factory
              ->QueryCodecSupport(config.format, config.scalability_mode)
              .is_power_efficient;
    }

    base::UmaHistogramSparse("WebRTC.Video.HwCapabilities." + GetTierName(),
                             uma_metric);
  }
}

void WaitForEncoderSupportReady(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::DecoderFactory* media_decoder_factory) {
  gpu_factories->NotifyEncoderSupportKnown(
      base::BindOnce(&ReportUmaEncodeDecodeCapabilities, gpu_factories,
                     media_decoder_factory));
}

}  // namespace

// static
const char PeerConnectionDependencyFactory::kSupplementName[] =
    "PeerConnectionDependencyFactory";

PeerConnectionDependencyFactory& PeerConnectionDependencyFactory::From(
    ExecutionContext& context) {
  CHECK(!context.IsContextDestroyed());
  auto* supplement =
      Supplement<ExecutionContext>::From<PeerConnectionDependencyFactory>(
          context);
  if (!supplement) {
    supplement = MakeGarbageCollected<PeerConnectionDependencyFactory>(
        context, PassKey());
    ProvideTo(context, supplement);
  }
  return *supplement;
}

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory(
    ExecutionContext& context,
    PassKey)
    : Supplement(context),
      ExecutionContextLifecycleObserver(&context),
      network_manager_(nullptr),
      p2p_socket_dispatcher_(P2PSocketDispatcher::From(context)) {}

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory()
    : Supplement(nullptr), ExecutionContextLifecycleObserver(nullptr) {}

PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() = default;

std::unique_ptr<RTCPeerConnectionHandler>
PeerConnectionDependencyFactory::CreateRTCPeerConnectionHandler(
    RTCPeerConnectionHandlerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool force_encoded_audio_insertable_streams,
    bool force_encoded_video_insertable_streams) {
  // Save histogram data so we can see how much PeerConnection is used.
  // The histogram counts the number of calls to the JS API
  // RTCPeerConnection.
  UpdateWebRTCMethodCount(RTCAPIName::kRTCPeerConnection);

  return std::make_unique<RTCPeerConnectionHandler>(
      client, this, task_runner, force_encoded_audio_insertable_streams,
      force_encoded_video_insertable_streams);
}

const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
PeerConnectionDependencyFactory::GetPcFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!pc_factory_)
    CreatePeerConnectionFactory();
  CHECK(pc_factory_);
  return pc_factory_;
}

void PeerConnectionDependencyFactory::CreatePeerConnectionFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!pc_factory_.get());
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);

  DVLOG(1) << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";

  StaticDeps().EnsureChromeThreadsStarted();
  base::WaitableEvent& worker_thread_started_event =
      StaticDeps().InitializeWorkerThread();
  StaticDeps().InitializeNetworkThread();
  StaticDeps().InitializeSignalingThread();

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  // Building /w |rtc_use_h264|, is the corresponding run-time feature enabled?
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    // Feature is to be disabled.
    webrtc::DisableRtcUseH264();
  }
#else
  webrtc::DisableRtcUseH264();
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

  EnsureWebRtcAudioDeviceImpl();

  // Init SSL, which will be needed by PeerConnection.
  if (!rtc::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    NOTREACHED();
    return;
  }

  base::WaitableEvent create_network_manager_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<MdnsResponderAdapter> mdns_responder;
#if BUILDFLAG(ENABLE_MDNS)
  if (base::FeatureList::IsEnabled(
          blink::features::kWebRtcHideLocalIpsWithMdns)) {
    // Note that MdnsResponderAdapter is created on the main thread to have
    // access to the connector to the service manager.
    mdns_responder =
        std::make_unique<MdnsResponderAdapter>(*GetSupplementable());
  }
#endif  // BUILDFLAG(ENABLE_MDNS)
  PostCrossThreadTask(
      *GetWebRtcNetworkTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&PeerConnectionDependencyFactory::
                              CreateIpcNetworkManagerOnNetworkThread,
                          WrapCrossThreadPersistent(this),
                          CrossThreadUnretained(&create_network_manager_event),
                          std::move(mdns_responder)));

  create_network_manager_event.Wait();
  CHECK(GetNetworkThread());

  // Wait for the worker thread, since `InitializeSignalingThread` needs to
  // refer to `worker_thread_`.
  worker_thread_started_event.Wait();
  CHECK(!GetChromeWorkerThread() || GetWorkerThread());

  if (!metronome_source_ &&
      base::FeatureList::IsEnabled(kWebRtcMetronomeTaskQueue)) {
    // Store a reference to the context's metronome provider so that it can be
    // used when cleaning up peer connections, even while the context is being
    // destroyed.
    metronome_provider_ =
        ExecutionContextMetronomeProvider::From(*GetExecutionContext())
            .metronome_provider();
    DCHECK(metronome_provider_);
    metronome_source_ = base::MakeRefCounted<MetronomeSource>(
        kWebRtcMetronomeTaskQueueTick.Get());
  }

  base::WaitableEvent start_signaling_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *GetChromeSignalingThread().task_runner(), FROM_HERE,
      CrossThreadBindOnce(
          &PeerConnectionDependencyFactory::InitializeSignalingThread,
          WrapCrossThreadPersistent(this),
          Platform::Current()->GetRenderingColorSpace(),
          Platform::Current()->MediaThreadTaskRunner(),
          CrossThreadUnretained(Platform::Current()->GetGpuFactories()),
          CrossThreadUnretained(Platform::Current()->GetMediaDecoderFactory()),
          CrossThreadUnretained(&start_signaling_event)));

  start_signaling_event.Wait();

  CHECK(pc_factory_);
  CHECK(socket_factory_);
  CHECK(GetSignalingThread());
}

void PeerConnectionDependencyFactory::InitializeSignalingThread(
    const gfx::ColorSpace& render_color_space,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::DecoderFactory* media_decoder_factory,
    base::WaitableEvent* event) {
  DCHECK(GetChromeSignalingThread().task_runner()->BelongsToCurrentThread());
  DCHECK(GetNetworkThread());
  // The task to initialize `signaling_thread_` was posted to the same thread,
  // so there is no need to wait on its event.
  DCHECK(GetSignalingThread());
  DCHECK(p2p_socket_dispatcher_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("webrtc_peer_connection", R"(
        semantics {
          sender: "WebRTC"
          description:
            "WebRTC is an API that provides web applications with Real Time "
            "Communication (RTC) capabilities. It is used to establish a "
            "secure session with a remote peer, transmitting and receiving "
            "audio, video and potentially other data."
          trigger:
            "Application creates an RTCPeerConnection and connects it to a "
            "remote peer by exchanging an SDP offer and answer."
          data:
            "Media encrypted using DTLS-SRTP, and protocol-level messages for "
            "the various subprotocols employed by WebRTC (including ICE, DTLS, "
            "RTCP, etc.). Note that ICE connectivity checks may leak the "
            "user's IP address(es), subject to the restrictions/guidance in "
            "https://datatracker.ietf.org/doc/draft-ietf-rtcweb-ip-handling."
          destination: OTHER
          destination_other:
            "A destination determined by the web application that created the "
            "connection."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but it won't be used "
            "unless the application creates an RTCPeerConnection. Media can "
            "only be captured with user's consent, but data may be sent "
            "withouth that."
          policy_exception_justification:
            "Not implemented. 'WebRtcUdpPortRange' policy can limit the range "
            "of ports used by WebRTC, but there is no policy to generally "
            "block it."
        }
    )");
  socket_factory_ = std::make_unique<IpcPacketSocketFactory>(
      p2p_socket_dispatcher_.Get(), traffic_annotation);

  gpu_factories_ = gpu_factories;
  std::unique_ptr<webrtc::VideoEncoderFactory> webrtc_encoder_factory =
      blink::CreateWebrtcVideoEncoderFactory(gpu_factories);
  std::unique_ptr<webrtc::VideoDecoderFactory> webrtc_decoder_factory =
      blink::CreateWebrtcVideoDecoderFactory(
          gpu_factories, media_decoder_factory, std::move(media_task_runner),
          render_color_space);

  if (!encode_decode_capabilities_reported_) {
    encode_decode_capabilities_reported_ = true;
    if (gpu_factories) {
      // Wait until decoder and encoder support are known.
      gpu_factories->NotifyDecoderSupportKnown(base::BindOnce(
          &WaitForEncoderSupportReady, gpu_factories, media_decoder_factory));
    } else {
      ReportUmaEncodeDecodeCapabilities(gpu_factories, media_decoder_factory);
    }
  }

  // Enable Multiplex codec in SDP optionally.
  if (base::FeatureList::IsEnabled(blink::features::kWebRtcMultiplexCodec)) {
    webrtc_encoder_factory = std::make_unique<webrtc::MultiplexEncoderFactory>(
        std::move(webrtc_encoder_factory));
    webrtc_decoder_factory = std::make_unique<webrtc::MultiplexDecoderFactory>(
        std::move(webrtc_decoder_factory));
  }

  if (blink::Platform::Current()->UsesFakeCodecForPeerConnection()) {
    webrtc_encoder_factory =
        std::make_unique<webrtc::FakeVideoEncoderFactory>();
    webrtc_decoder_factory =
        std::make_unique<webrtc::FakeVideoDecoderFactory>();
  }

  webrtc::PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.worker_thread =
      GetWorkerThread() ? GetWorkerThread() : GetSignalingThread();
  pcf_deps.signaling_thread = GetSignalingThread();
  pcf_deps.network_thread = GetNetworkThread();
  pcf_deps.task_queue_factory =
      !metronome_source_
          ? CreateWebRtcTaskQueueFactory()
          : CreateWebRtcMetronomeTaskQueueFactory(metronome_source_);
  pcf_deps.call_factory = webrtc::CreateCallFactory();
  pcf_deps.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>(
      pcf_deps.task_queue_factory.get());
  cricket::MediaEngineDependencies media_deps;
  media_deps.task_queue_factory = pcf_deps.task_queue_factory.get();
  media_deps.adm = audio_device_.get();
  media_deps.audio_encoder_factory = blink::CreateWebrtcAudioEncoderFactory();
  media_deps.audio_decoder_factory = blink::CreateWebrtcAudioDecoderFactory();
  media_deps.video_encoder_factory = std::move(webrtc_encoder_factory);
  media_deps.video_decoder_factory = std::move(webrtc_decoder_factory);
  // Audio Processing Module (APM) instances are owned and handled by the Blink
  // media stream module.
  DCHECK_EQ(media_deps.audio_processing, nullptr);
  pcf_deps.media_engine = cricket::CreateMediaEngine(std::move(media_deps));
  pc_factory_ = webrtc::CreateModularPeerConnectionFactory(std::move(pcf_deps));
  CHECK(pc_factory_.get());

  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.disable_encryption =
      !blink::Platform::Current()->IsWebRtcEncryptionEnabled();
  pc_factory_->SetOptions(factory_options);

  event->Signal();
}

bool PeerConnectionDependencyFactory::PeerConnectionFactoryCreated() {
  return !!pc_factory_;
}

scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    blink::WebLocalFrame* web_frame,
    webrtc::PeerConnectionObserver* observer,
    ExceptionState& exception_state) {
  CHECK(observer);
  if (!GetPcFactory().get())
    return nullptr;

  webrtc::PeerConnectionDependencies dependencies(observer);
  // |web_frame| may be null in tests, e.g. if
  // RTCPeerConnectionHandler::InitializeForTest() is used.
  if (web_frame) {
    rtc::SetAllowLegacyTLSProtocols(
        web_frame->Client()->AllowRTCLegacyTLSProtocols());
    dependencies.allocator = CreatePortAllocator(web_frame);
  }
  dependencies.async_resolver_factory = CreateAsyncResolverFactory();
  auto pc_or_error = GetPcFactory()->CreatePeerConnectionOrError(
      config, std::move(dependencies));
  if (pc_or_error.ok()) {
    ++open_peer_connections_;
    if (open_peer_connections_ == 1u && metronome_source_) {
      metronome_provider_->OnStartUsingMetronome(metronome_source_);
    }
    // Convert from rtc::scoped_refptr to scoped_refptr
    return pc_or_error.value().get();
  } else {
    // Convert error
    ThrowExceptionFromRTCError(pc_or_error.error(), exception_state);
    return nullptr;
  }
}

size_t PeerConnectionDependencyFactory::open_peer_connections() const {
  return open_peer_connections_;
}

void PeerConnectionDependencyFactory::OnPeerConnectionClosed() {
  DCHECK(open_peer_connections_);
  --open_peer_connections_;
  if (!open_peer_connections_ && metronome_source_) {
    metronome_provider_->OnStopUsingMetronome();
  }
}

scoped_refptr<MetronomeProvider>
PeerConnectionDependencyFactory::metronome_provider() const {
  return metronome_provider_;
}

std::unique_ptr<cricket::PortAllocator>
PeerConnectionDependencyFactory::CreatePortAllocator(
    blink::WebLocalFrame* web_frame) {
  DCHECK(web_frame);
  EnsureInitialized();

  // Copy the flag from Preference associated with this WebLocalFrame.
  P2PPortAllocator::Config port_config;
  uint16_t min_port = 0;
  uint16_t max_port = 0;
  bool allow_mdns_obfuscation = true;

  // |media_permission| will be called to check mic/camera permission. If at
  // least one of them is granted, P2PPortAllocator is allowed to gather local
  // host IP addresses as ICE candidates. |media_permission| could be nullptr,
  // which means the permission will be granted automatically. This could be the
  // case when either the experiment is not enabled or the preference is not
  // enforced.
  //
  // Note on |media_permission| lifetime: |media_permission| is owned by a frame
  // (RenderFrameImpl). It is also stored as an indirect member of
  // RTCPeerConnectionHandler (through PeerConnection/PeerConnectionInterface ->
  // P2PPortAllocator -> FilteringNetworkManager -> |media_permission|).
  // The RTCPeerConnectionHandler is owned as RTCPeerConnection::m_peerHandler
  // in Blink, which will be reset in RTCPeerConnection::stop(). Since
  // ActiveDOMObject::stop() is guaranteed to be called before a frame is
  // detached, it is impossible for RTCPeerConnectionHandler to outlive the
  // frame. Therefore using a raw pointer of |media_permission| is safe here.
  media::MediaPermission* media_permission = nullptr;
  if (!Platform::Current()->ShouldEnforceWebRTCRoutingPreferences()) {
    port_config.enable_multiple_routes = true;
    port_config.enable_nonproxied_udp = true;
    VLOG(3) << "WebRTC routing preferences will not be enforced";
  } else {
    if (web_frame && web_frame->View()) {
      WebString webrtc_ip_handling_policy;
      Platform::Current()->GetWebRTCRendererPreferences(
          web_frame, &webrtc_ip_handling_policy, &min_port, &max_port,
          &allow_mdns_obfuscation);

      // TODO(guoweis): |enable_multiple_routes| should be renamed to
      // |request_multiple_routes|. Whether local IP addresses could be
      // collected depends on if mic/camera permission is granted for this
      // origin.
      WebRTCIPHandlingPolicy policy =
          GetWebRTCIPHandlingPolicy(webrtc_ip_handling_policy);
      switch (policy) {
        // TODO(guoweis): specify the flag of disabling local candidate
        // collection when webrtc is updated.
        case DEFAULT_PUBLIC_INTERFACE_ONLY:
        case DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = true;
          port_config.enable_default_local_candidate =
              (policy == DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES);
          break;
        case DISABLE_NON_PROXIED_UDP:
          port_config.enable_multiple_routes = false;
          port_config.enable_nonproxied_udp = false;
          break;
        case DEFAULT:
          port_config.enable_multiple_routes = true;
          port_config.enable_nonproxied_udp = true;
          break;
      }

      VLOG(3) << "WebRTC routing preferences: "
              << "policy: " << policy
              << ", multiple_routes: " << port_config.enable_multiple_routes
              << ", nonproxied_udp: " << port_config.enable_nonproxied_udp
              << ", min_udp_port: " << min_port
              << ", max_udp_port: " << max_port
              << ", allow_mdns_obfuscation: " << allow_mdns_obfuscation;
    }
    if (port_config.enable_multiple_routes) {
      media_permission =
          blink::Platform::Current()->GetWebRTCMediaPermission(web_frame);
    }
  }

  // Now that this file is within Blink, it can not rely on WebURL's
  // GURL() operator directly. Hence, as per the comment on gurl.h, the
  // following GURL ctor is used instead.
  WebURL document_url = web_frame->GetDocument().Url();
  const GURL& requesting_origin =
      GURL(document_url.GetString().Utf8(), document_url.GetParsed(),
           document_url.IsValid())
          .DeprecatedGetOriginAsURL();

  std::unique_ptr<rtc::NetworkManager> network_manager;
  if (port_config.enable_multiple_routes) {
    network_manager = std::make_unique<FilteringNetworkManager>(
        network_manager_.get(), media_permission, allow_mdns_obfuscation);
  } else {
    network_manager =
        std::make_unique<blink::EmptyNetworkManager>(network_manager_.get());
  }
  auto port_allocator = std::make_unique<P2PPortAllocator>(
      std::move(network_manager), socket_factory_.get(), port_config,
      requesting_origin);
  if (IsValidPortRange(min_port, max_port))
    port_allocator->SetPortRange(min_port, max_port);

  return port_allocator;
}

std::unique_ptr<webrtc::AsyncResolverFactory>
PeerConnectionDependencyFactory::CreateAsyncResolverFactory() {
  EnsureInitialized();
  return std::make_unique<ProxyAsyncResolverFactory>(socket_factory_.get());
}

scoped_refptr<webrtc::MediaStreamInterface>
PeerConnectionDependencyFactory::CreateLocalMediaStream(const String& label) {
  return GetPcFactory()->CreateLocalMediaStream(label.Utf8()).get();
}

scoped_refptr<webrtc::VideoTrackSourceInterface>
PeerConnectionDependencyFactory::CreateVideoTrackSourceProxy(
    webrtc::VideoTrackSourceInterface* source) {
  // PeerConnectionFactory needs to be instantiated to make sure that
  // signaling_thread_ and network_thread_ exist.
  if (!PeerConnectionFactoryCreated())
    CreatePeerConnectionFactory();

  return webrtc::CreateVideoTrackSourceProxy(GetSignalingThread(),
                                             GetNetworkThread(), source)
      .get();
}

scoped_refptr<webrtc::VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const String& id,
    webrtc::VideoTrackSourceInterface* source) {
  return GetPcFactory()->CreateVideoTrack(id.Utf8(), source).get();
}

webrtc::IceCandidateInterface*
PeerConnectionDependencyFactory::CreateIceCandidate(const String& sdp_mid,
                                                    int sdp_mline_index,
                                                    const String& sdp) {
  return webrtc::CreateIceCandidate(sdp_mid.Utf8(), sdp_mline_index, sdp.Utf8(),
                                    nullptr);
}

blink::WebRtcAudioDeviceImpl*
PeerConnectionDependencyFactory::GetWebRtcAudioDevice() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureWebRtcAudioDeviceImpl();
  return audio_device_.get();
}

void PeerConnectionDependencyFactory::CreateIpcNetworkManagerOnNetworkThread(
    base::WaitableEvent* event,
    std::unique_ptr<MdnsResponderAdapter> mdns_responder) {
  DCHECK(GetChromeNetworkThread().task_runner()->BelongsToCurrentThread());
  // The task to initialize `network_thread_` was posted to the same thread, so
  // there is no need to wait on its event.
  DCHECK(GetNetworkThread());

  network_manager_ = std::make_unique<blink::IpcNetworkManager>(
      p2p_socket_dispatcher_.Get(), std::move(mdns_responder));

  event->Signal();
}

void PeerConnectionDependencyFactory::DeleteIpcNetworkManager(
    std::unique_ptr<IpcNetworkManager> network_manager,
    base::WaitableEvent* event) {
  DCHECK(GetChromeNetworkThread().task_runner()->BelongsToCurrentThread());
  network_manager = nullptr;
  event->Signal();
}

void PeerConnectionDependencyFactory::ContextDestroyed() {
  if (network_manager_) {
    PostCrossThreadTask(
        *GetWebRtcNetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&IpcNetworkManager::ContextDestroyed,
                            CrossThreadUnretained(network_manager_.get())));
  }
}

void PeerConnectionDependencyFactory::CleanupPeerConnectionFactory() {
  DVLOG(1) << "PeerConnectionDependencyFactory::CleanupPeerConnectionFactory()";
  socket_factory_ = nullptr;
  pc_factory_ = nullptr;
  if (network_manager_) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    // The network manager needs to free its resources on the thread they were
    // created, which is the network thread.
    PostCrossThreadTask(
        *GetWebRtcNetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            &PeerConnectionDependencyFactory::DeleteIpcNetworkManager,
            std::move(network_manager_), CrossThreadUnretained(&event)));
    network_manager_ = nullptr;
    event.Wait();
  }
}

void PeerConnectionDependencyFactory::EnsureInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetPcFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcNetworkTaskRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetChromeNetworkThread().IsRunning()
             ? GetChromeNetworkThread().task_runner()
             : nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcSignalingTaskRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureInitialized();
  return GetChromeSignalingThread().IsRunning()
             ? GetChromeSignalingThread().task_runner()
             : nullptr;
}

void PeerConnectionDependencyFactory::EnsureWebRtcAudioDeviceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (audio_device_.get())
    return;

  audio_device_ = new rtc::RefCountedObject<blink::WebRtcAudioDeviceImpl>();
}

std::unique_ptr<webrtc::RtpCapabilities>
PeerConnectionDependencyFactory::GetSenderCapabilities(const String& kind) {
  if (kind == "audio") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_AUDIO));
  } else if (kind == "video") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO));
  }
  return nullptr;
}

std::unique_ptr<webrtc::RtpCapabilities>
PeerConnectionDependencyFactory::GetReceiverCapabilities(const String& kind) {
  if (kind == "audio") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_AUDIO));
  } else if (kind == "video") {
    return std::make_unique<webrtc::RtpCapabilities>(
        GetPcFactory()->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_VIDEO));
  }
  return nullptr;
}

media::GpuVideoAcceleratorFactories*
PeerConnectionDependencyFactory::GetGpuFactories() {
  return gpu_factories_;
}

void PeerConnectionDependencyFactory::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(p2p_socket_dispatcher_);
}
}  // namespace blink
