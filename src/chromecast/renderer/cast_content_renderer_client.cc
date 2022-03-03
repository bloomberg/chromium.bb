// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/base/bitstream_audio_codecs.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/crash/app_state_tracker.h"
#include "chromecast/media/base/media_codec_support.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/renderer/cast_url_loader_throttle_provider.h"
#include "chromecast/renderer/cast_websocket_handshake_throttle_provider.h"
#include "chromecast/renderer/identification_settings_manager_renderer.h"
#include "chromecast/renderer/js_channel_bindings.h"
#include "chromecast/renderer/media/key_systems_cast.h"
#include "chromecast/renderer/media/media_caps_observer_impl.h"
#include "components/media_control/renderer/media_playback_options.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/on_load_script_injector/renderer/on_load_script_injector.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "media/base/audio_parameters.h"
#include "media/base/media.h"
#include "media/remoting/receiver_controller.h"
#include "media/remoting/remoting_constants.h"
#include "media/remoting/stream_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

#if defined(OS_ANDROID)
#include "chromecast/media/audio/cast_audio_device_factory.h"
#include "media/base/android/media_codec_util.h"
#else
#include "chromecast/renderer/memory_pressure_observer_impl.h"
#endif  // OS_ANDROID

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/common/cast_extensions_client.h"
#include "chromecast/renderer/cast_extensions_renderer_client.h"
#include "components/guest_view/renderer/guest_view_container_dispatcher.h"
#include "content/public/common/content_constants.h"
#include "extensions/common/common_manifest_handlers.h"  // nogncheck
#include "extensions/common/extension_urls.h"            // nogncheck
#include "extensions/renderer/dispatcher.h"              // nogncheck
#include "extensions/renderer/extension_frame_helper.h"  // nogncheck
#endif

namespace chromecast {
namespace shell {
namespace {
bool IsSupportedBitstreamAudioCodecHelper(::media::AudioCodec codec, int mask) {
  return (codec == ::media::AudioCodec::kAC3 &&
          (kBitstreamAudioCodecAc3 & mask)) ||
         (codec == ::media::AudioCodec::kEAC3 &&
          (kBitstreamAudioCodecEac3 & mask)) ||
         (codec == ::media::AudioCodec::kMpegHAudio &&
          (kBitstreamAudioCodecMpegHAudio & mask));
}
}  // namespace

#if defined(OS_ANDROID)
// Audio renderer algorithm maximum capacity. 5s buffer is already large enough,
// we don't need a larger capacity. Otherwise audio renderer will double the
// buffer size when underrun happens, which will cause the playback paused to
// wait long time for enough buffers.
constexpr base::TimeDelta kAudioRendererMaxCapacity = base::Seconds(5);
// Audio renderer algorithm starting capacity.  Configure large enough to
// prevent underrun.
constexpr base::TimeDelta kAudioRendererStartingCapacity = base::Seconds(5);
constexpr base::TimeDelta kAudioRendererStartingCapacityEncrypted =
    base::Seconds(5);
#endif  // defined(OS_ANDROID)

CastContentRendererClient::CastContentRendererClient()
    : supported_profiles_(
          std::make_unique<media::SupportedCodecProfileLevelsMemo>()),
      activity_url_filter_manager_(
          std::make_unique<CastActivityUrlFilterManager>()) {
#if defined(OS_ANDROID)
  // Registers a custom content::AudioDeviceFactory
  cast_audio_device_factory_ =
      std::make_unique<media::CastAudioDeviceFactory>();
#endif  // OS_ANDROID
}

CastContentRendererClient::~CastContentRendererClient() = default;

void CastContentRendererClient::RenderThreadStarted() {
  // Register as observer for media capabilities
  content::RenderThread* thread = content::RenderThread::Get();
  mojo::Remote<media::mojom::MediaCaps> media_caps;
  thread->BindHostReceiver(media_caps.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<media::mojom::MediaCapsObserver> proxy;
  media_caps_observer_.reset(
      new media::MediaCapsObserverImpl(&proxy, supported_profiles_.get()));
  media_caps->AddObserver(std::move(proxy));

#if !defined(OS_ANDROID)
  // Register to observe memory pressure changes
  mojo::Remote<chromecast::mojom::MemoryPressureController>
      memory_pressure_controller;
  thread->BindHostReceiver(
      memory_pressure_controller.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<chromecast::mojom::MemoryPressureObserver>
      memory_pressure_proxy;
  memory_pressure_observer_.reset(
      new MemoryPressureObserverImpl(&memory_pressure_proxy));
  memory_pressure_controller->AddObserver(std::move(memory_pressure_proxy));
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string last_launched_app =
      command_line->GetSwitchValueNative(switches::kLastLaunchedApp);
  if (!last_launched_app.empty())
    AppStateTracker::SetLastLaunchedApp(last_launched_app);

  std::string previous_app =
      command_line->GetSwitchValueNative(switches::kPreviousApp);
  if (!previous_app.empty())
    AppStateTracker::SetPreviousApp(previous_app);

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  extensions_client_ = std::make_unique<extensions::CastExtensionsClient>();
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_renderer_client_ =
      std::make_unique<extensions::CastExtensionsRendererClient>();
  extensions::ExtensionsRendererClient::Set(extensions_renderer_client_.get());

  thread->AddObserver(extensions_renderer_client_->GetDispatcher());

  guest_view_container_dispatcher_ =
      std::make_unique<guest_view::GuestViewContainerDispatcher>();
  thread->AddObserver(guest_view_container_dispatcher_.get());
#endif
}

void CastContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame);

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  if (render_frame->IsMainFrame()) {
    if (main_frame_feature_manager_on_associated_interface_) {
      LOG(DFATAL) << "main_frame_feature_manager_on_associated_interface_ gets "
                     "overwritten.";
    }
    main_frame_feature_manager_on_associated_interface_ =
        new FeatureManagerOnAssociatedInterface(render_frame);
  } else {
    new FeatureManagerOnAssociatedInterface(render_frame);
  }
  new media_control::MediaPlaybackOptions(render_frame);

  // Add script injection support to the RenderFrame, used by Cast platform
  // APIs. The injector's lifetime is bound to the RenderFrame's lifetime.
  new on_load_script_injector::OnLoadScriptInjector(render_frame);

  if (!app_media_capabilities_observer_receiver_.is_bound()) {
    mojo::Remote<mojom::ApplicationMediaCapabilities> app_media_capabilities;
    render_frame->GetBrowserInterfaceBroker()->GetInterface(
        app_media_capabilities.BindNewPipeAndPassReceiver());
    app_media_capabilities->AddObserver(
        app_media_capabilities_observer_receiver_.BindNewPipeAndPassRemote());
  }

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  extensions::Dispatcher* dispatcher =
      extensions_renderer_client_->GetDispatcher();
  // ExtensionFrameHelper destroys itself when the RenderFrame is destroyed.
  new extensions::ExtensionFrameHelper(render_frame, dispatcher);

  dispatcher->OnRenderFrameCreated(render_frame);
#endif

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(USE_OZONE)
  // JsChannelBindings destroys itself when the RenderFrame is destroyed.
  JsChannelBindings::Create(render_frame);
#endif

  activity_url_filter_manager_->OnRenderFrameCreated(render_frame);

  // |base::Unretained| is safe here since the callback is triggered before the
  // destruction of IdentificationSettingsManager by which point
  // CastContentRendererClient should be alive.
  settings_managers_.emplace(
      render_frame->GetRoutingID(),
      base::MakeRefCounted<IdentificationSettingsManagerRenderer>(
          render_frame,
          base::BindOnce(&CastContentRendererClient::OnRenderFrameRemoved,
                         base::Unretained(this),
                         render_frame->GetRoutingID())));
}

void CastContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  extensions_renderer_client_->GetDispatcher()->RunScriptsAtDocumentStart(
      render_frame);
#endif
}

void CastContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  extensions_renderer_client_->GetDispatcher()->RunScriptsAtDocumentEnd(
      render_frame);
#endif
}

void CastContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>*
        key_systems_properties) {
  media::AddChromecastKeySystems(key_systems_properties,
                                 false /* enable_persistent_license_support */,
                                 false /* enable_playready */);
}

bool CastContentRendererClient::IsSupportedAudioType(
    const ::media::AudioType& type) {
#if defined(OS_ANDROID)
  if (type.spatial_rendering)
    return false;

  // No ATV device we know of has (E)AC3 decoder, so it relies on the audio sink
  // device.
  if (type.codec == ::media::AudioCodec::kEAC3) {
    return kBitstreamAudioCodecEac3 &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::AudioCodec::kAC3) {
    return kBitstreamAudioCodecAc3 &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::AudioCodec::kMpegHAudio) {
    return kBitstreamAudioCodecMpegHAudio &
           supported_bitstream_audio_codecs_info_.codecs;
  }

  return ::media::IsDefaultSupportedAudioType(type);
#else
  if (type.profile == ::media::AudioCodecProfile::kXHE_AAC)
    return false;

  // If the HDMI sink supports bitstreaming the codec, then the vendor backend
  // does not need to support it.
  if (CheckSupportedBitstreamAudioCodec(type.codec, type.spatial_rendering))
    return true;

  media::AudioCodec codec = media::ToCastAudioCodec(type.codec);
  // Cast platform implements software decoding of Opus and FLAC, so only PCM
  // support is necessary in order to support Opus and FLAC.
  if (codec == media::kCodecOpus || codec == media::kCodecFLAC)
    codec = media::kCodecPCM;

  media::AudioConfig cast_audio_config;
  cast_audio_config.codec = codec;
  return media::MediaCapabilitiesShlib::IsSupportedAudioConfig(
      cast_audio_config);
#endif
}

bool CastContentRendererClient::IsSupportedVideoType(
    const ::media::VideoType& type) {
// TODO(servolk): make use of eotf.

  // TODO(1066567): Check attached screen for support of type.hdr_metadata_type.
if (type.hdr_metadata_type != ::gfx::HdrMetadataType::kNone) {
  NOTIMPLEMENTED() << "HdrMetadataType support signaling not implemented.";
  return false;
}

#if defined(OS_ANDROID)
  return supported_profiles_->IsSupportedVideoConfig(
      media::ToCastVideoCodec(type.codec, type.profile),
      media::ToCastVideoProfile(type.profile), type.level);
#else
  return media::MediaCapabilitiesShlib::IsSupportedVideoConfig(
      media::ToCastVideoCodec(type.codec, type.profile),
      media::ToCastVideoProfile(type.profile), type.level);
#endif
}

bool CastContentRendererClient::IsSupportedBitstreamAudioCodec(
    ::media::AudioCodec codec) {
  return IsSupportedBitstreamAudioCodecHelper(
      codec, supported_bitstream_audio_codecs_info_.codecs);
}

bool CastContentRendererClient::CheckSupportedBitstreamAudioCodec(
    ::media::AudioCodec codec,
    bool check_spatial_rendering) {
  if (!IsSupportedBitstreamAudioCodec(codec))
    return false;

  if (!check_spatial_rendering)
    return true;

  return IsSupportedBitstreamAudioCodecHelper(
      codec, supported_bitstream_audio_codecs_info_.spatial_rendering);
}

std::unique_ptr<blink::WebPrescientNetworking>
CastContentRendererClient::CreatePrescientNetworking(
    content::RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

bool CastContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool render_frame_has_played_media_before,
    base::OnceClosure closure) {
  return RunWhenInForeground(render_frame, std::move(closure));
}

std::unique_ptr<::media::Demuxer>
CastContentRendererClient::OverrideDemuxerForUrl(
    content::RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (render_frame->GetRenderFrameMediaPlaybackOptions()
          .is_remoting_renderer_enabled() &&
      url.SchemeIs(::media::remoting::kRemotingScheme)) {
    return std::make_unique<::media::remoting::StreamProvider>(
        ::media::remoting::ReceiverController::GetInstance(), task_runner);
  }
  return nullptr;
}

bool CastContentRendererClient::RunWhenInForeground(
    content::RenderFrame* render_frame,
    base::OnceClosure closure) {
  auto* playback_options =
      media_control::MediaPlaybackOptions::Get(render_frame);
  DCHECK(playback_options);
  return playback_options->RunWhenInForeground(std::move(closure));
}

bool CastContentRendererClient::IsIdleMediaSuspendEnabled() {
  return false;
}

void CastContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // Allow HtmlMediaElement.volume to be greater than 1, for normalization.
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "MediaElementVolumeGreaterThanOne", true);
  // Settings for ATV (Android defaults are not what we want).
  blink::WebRuntimeFeatures::EnableMediaControlsOverlayPlayButton(false);
}

void CastContentRendererClient::OnSupportedBitstreamAudioCodecsChanged(
    const BitstreamAudioCodecsInfo& info) {
  supported_bitstream_audio_codecs_info_ = info;
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
CastContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<CastWebSocketHandshakeThrottleProvider>(
      activity_url_filter_manager_.get());
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
CastContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type) {
  return std::make_unique<CastURLLoaderThrottleProvider>(
      type, activity_url_filter_manager(), this);
}

absl::optional<::media::AudioRendererAlgorithmParameters>
CastContentRendererClient::GetAudioRendererAlgorithmParameters(
    ::media::AudioParameters audio_parameters) {
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(kEnableCastAudioOutputDevice)) {
    return absl::nullopt;
  }
  ::media::AudioRendererAlgorithmParameters parameters;
  parameters.max_capacity = kAudioRendererMaxCapacity;
  parameters.starting_capacity = kAudioRendererStartingCapacity;
  parameters.starting_capacity_for_encrypted =
      kAudioRendererStartingCapacityEncrypted;
  return absl::optional<::media::AudioRendererAlgorithmParameters>(parameters);
#else
  return absl::nullopt;
#endif
}

scoped_refptr<IdentificationSettingsManager>
CastContentRendererClient::GetSettingsManagerFromRenderFrameID(
    int render_frame_id) {
  const auto& it = settings_managers_.find(render_frame_id);
  if (it == settings_managers_.end()) {
    return nullptr;
  }
  return it->second;
}

void CastContentRendererClient::OnRenderFrameRemoved(int render_frame_id) {
  size_t result = settings_managers_.erase(render_frame_id);
  if (result != 1U) {
    LOG(WARNING)
        << "Can't find the identification settings manager for render frame: "
        << render_frame_id;
  }
}

}  // namespace shell
}  // namespace chromecast
