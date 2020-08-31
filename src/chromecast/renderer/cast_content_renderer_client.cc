// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/optional.h"
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
#include "chromecast/renderer/js_channel_bindings.h"
#include "chromecast/renderer/media/key_systems_cast.h"
#include "chromecast/renderer/media/media_caps_observer_impl.h"
#include "chromecast/renderer/on_load_script_injector.h"
#include "chromecast/renderer/queryable_data_bindings.h"
#include "components/media_control/renderer/media_playback_options.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "media/base/audio_parameters.h"
#include "media/base/media.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
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
#include "content/public/common/content_constants.h"
#include "extensions/common/common_manifest_handlers.h"  // nogncheck
#include "extensions/common/extension_urls.h"            // nogncheck
#include "extensions/renderer/dispatcher.h"              // nogncheck
#include "extensions/renderer/extension_frame_helper.h"  // nogncheck
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"  // nogncheck
#include "extensions/renderer/guest_view/extensions_guest_view_container_dispatcher.h"  // nogncheck
#endif

namespace chromecast {
namespace shell {
namespace {
bool IsSupportedBitstreamAudioCodecHelper(::media::AudioCodec codec, int mask) {
  return (codec == ::media::kCodecAC3 && (kBitstreamAudioCodecAc3 & mask)) ||
         (codec == ::media::kCodecEAC3 && (kBitstreamAudioCodecEac3 & mask)) ||
         (codec == ::media::kCodecMpegHAudio &&
          (kBitstreamAudioCodecMpegHAudio & mask));
}
}  // namespace

#if defined(OS_ANDROID)
// Audio renderer algorithm maximum capacity.
constexpr base::TimeDelta kAudioRendererMaxCapacity =
    base::TimeDelta::FromSeconds(10);
// Audio renderer algorithm starting capacity.  Configure large enough to
// prevent underrun.
constexpr base::TimeDelta kAudioRendererStartingCapacity =
    base::TimeDelta::FromMilliseconds(5000);
constexpr base::TimeDelta kAudioRendererStartingCapacityEncrypted =
    base::TimeDelta::FromMilliseconds(5500);
#endif  // defined(OS_ANDROID)

CastContentRendererClient::CastContentRendererClient()
    : supported_profiles_(
          std::make_unique<media::SupportedCodecProfileLevelsMemo>()),
      activity_url_filter_manager_(
          std::make_unique<CastActivityUrlFilterManager>()) {
#if defined(OS_ANDROID)
  DCHECK(::media::MediaCodecUtil::IsMediaCodecAvailable())
      << "MediaCodec is not available!";
  // Platform decoder support must be enabled before we set the
  // IsCodecSupportedCB because the latter instantiates the lazy MimeUtil
  // instance, which caches the platform decoder supported state when it is
  // constructed.
  ::media::EnablePlatformDecoderSupport();

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
      std::make_unique<extensions::ExtensionsGuestViewContainerDispatcher>();
  thread->AddObserver(guest_view_container_dispatcher_.get());
#endif

  for (auto& origin_or_hostname_pattern :
       network::SecureOriginAllowlist::GetInstance().GetCurrentAllowlist()) {
    blink::WebSecurityPolicy::AddOriginToTrustworthySafelist(
        blink::WebString::FromUTF8(origin_or_hostname_pattern));
  }
}

void CastContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  blink::WebView* webview = render_view->GetWebView();
  webview->SetBaseBackgroundColor(chromecast::GetSwitchValueColor(
      switches::kCastAppBackgroundColor, SK_ColorBLACK));
  // Disable application cache as Chromecast doesn't support off-line
  // application running.
  webview->GetSettings()->SetOfflineWebApplicationCacheEnabled(false);
}

void CastContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame);

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new media_control::MediaPlaybackOptions(render_frame);
  if (!::chromecast::IsFeatureEnabled(kUseQueryableDataBackend)) {
    new QueryableDataBindings(render_frame);
  }

  // Add script injection support to the RenderFrame, used by Cast platform
  // APIs. The objects' lifetimes are bound to the RenderFrame's lifetime.
  new OnLoadScriptInjector(render_frame);

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

#if defined(OS_LINUX) && defined(USE_OZONE)
  // JsChannelBindings destroys itself when the RenderFrame is destroyed.
  JsChannelBindings::Create(render_frame);
#endif

  activity_url_filter_manager_->OnRenderFrameCreated(render_frame);
}

content::BrowserPluginDelegate*
CastContentRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const content::WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url) {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  if (mime_type == content::kBrowserPluginMimeType) {
    return new extensions::ExtensionsGuestViewContainer(render_frame);
  }
#endif
  return nullptr;
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
                                 false /* force_software_crypto */);
}

bool CastContentRendererClient::IsSupportedAudioType(
    const ::media::AudioType& type) {
#if defined(OS_ANDROID)
  if (type.spatial_rendering)
    return false;

  // No ATV device we know of has (E)AC3 decoder, so it relies on the audio sink
  // device.
  if (type.codec == ::media::kCodecEAC3) {
    return kBitstreamAudioCodecEac3 &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::kCodecAC3) {
    return kBitstreamAudioCodecAc3 &
           supported_bitstream_audio_codecs_info_.codecs;
  }
  if (type.codec == ::media::kCodecMpegHAudio) {
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
  if (type.hdr_metadata_type != ::media::HdrMetadataType::kNone) {
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

std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
CastContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<CastWebSocketHandshakeThrottleProvider>(
      activity_url_filter_manager_.get());
}

std::unique_ptr<content::URLLoaderThrottleProvider>
CastContentRendererClient::CreateURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType type) {
  return std::make_unique<CastURLLoaderThrottleProvider>(
      type, activity_url_filter_manager_.get());
}

base::Optional<::media::AudioRendererAlgorithmParameters>
CastContentRendererClient::GetAudioRendererAlgorithmParameters(
    ::media::AudioParameters audio_parameters) {
#if defined(OS_ANDROID)
  ::media::AudioRendererAlgorithmParameters parameters;
  parameters.max_capacity = kAudioRendererMaxCapacity;
  parameters.starting_capacity = kAudioRendererStartingCapacity;
  parameters.starting_capacity_for_encrypted =
      kAudioRendererStartingCapacityEncrypted;
  return base::Optional<::media::AudioRendererAlgorithmParameters>(parameters);
#else
  return base::nullopt;
#endif
}

}  // namespace shell
}  // namespace chromecast
