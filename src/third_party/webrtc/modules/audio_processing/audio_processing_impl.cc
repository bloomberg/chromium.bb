/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/audio_processing_impl.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "common_audio/audio_converter.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/gain_applier.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/common.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

#define RETURN_ON_ERR(expr) \
  do {                      \
    int err = (expr);       \
    if (err != kNoError) {  \
      return err;           \
    }                       \
  } while (0)

namespace webrtc {

constexpr int AudioProcessing::kNativeSampleRatesHz[];
constexpr int kRuntimeSettingQueueSize = 100;

namespace {

static bool LayoutHasKeyboard(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      return false;
    case AudioProcessing::kMonoAndKeyboard:
    case AudioProcessing::kStereoAndKeyboard:
      return true;
  }

  RTC_NOTREACHED();
  return false;
}

bool SampleRateSupportsMultiBand(int sample_rate_hz) {
  return sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
         sample_rate_hz == AudioProcessing::kSampleRate48kHz;
}

// Checks whether the legacy ns functionality should be enforced.
bool DetectLegacyNsEnforcement() {
  return field_trial::IsEnabled("WebRTC-NewNoiseSuppressionKillSwitch");
}

// Checks whether AEC3 should be allowed to decide what the default
// configuration should be based on the render and capture channel configuration
// at hand.
bool UseSetupSpecificDefaultAec3Congfig() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3SetupSpecificDefaultConfigDefaultsKillSwitch");
}

// Identify the native processing rate that best handles a sample rate.
int SuitableProcessRate(int minimum_rate,
                        int max_splitting_rate,
                        bool band_splitting_required) {
  const int uppermost_native_rate =
      band_splitting_required ? max_splitting_rate : 48000;
  for (auto rate : {16000, 32000, 48000}) {
    if (rate >= uppermost_native_rate) {
      return uppermost_native_rate;
    }
    if (rate >= minimum_rate) {
      return rate;
    }
  }
  RTC_NOTREACHED();
  return uppermost_native_rate;
}

NoiseSuppression::Level NsConfigLevelToInterfaceLevel(
    AudioProcessing::Config::NoiseSuppression::Level level) {
  using NsConfig = AudioProcessing::Config::NoiseSuppression;
  switch (level) {
    case NsConfig::kLow:
      return NoiseSuppression::Level::kLow;
    case NsConfig::kModerate:
      return NoiseSuppression::Level::kModerate;
    case NsConfig::kHigh:
      return NoiseSuppression::Level::kHigh;
    case NsConfig::kVeryHigh:
      return NoiseSuppression::Level::kVeryHigh;
    default:
      RTC_NOTREACHED();
  }
}

GainControl::Mode Agc1ConfigModeToInterfaceMode(
    AudioProcessing::Config::GainController1::Mode mode) {
  using Agc1Config = AudioProcessing::Config::GainController1;
  switch (mode) {
    case Agc1Config::kAdaptiveAnalog:
      return GainControl::kAdaptiveAnalog;
    case Agc1Config::kAdaptiveDigital:
      return GainControl::kAdaptiveDigital;
    case Agc1Config::kFixedDigital:
      return GainControl::kFixedDigital;
  }
}

// Maximum lengths that frame of samples being passed from the render side to
// the capture side can have (does not apply to AEC3).
static const size_t kMaxAllowedValuesOfSamplesPerBand = 160;
static const size_t kMaxAllowedValuesOfSamplesPerFrame = 480;

// Maximum number of frames to buffer in the render queue.
// TODO(peah): Decrease this once we properly handle hugely unbalanced
// reverse and forward call numbers.
static const size_t kMaxNumFramesToBuffer = 100;
}  // namespace

// Throughout webrtc, it's assumed that success is represented by zero.
static_assert(AudioProcessing::kNoError == 0, "kNoError must be zero");

AudioProcessingImpl::SubmoduleStates::SubmoduleStates(
    bool capture_post_processor_enabled,
    bool render_pre_processor_enabled,
    bool capture_analyzer_enabled)
    : capture_post_processor_enabled_(capture_post_processor_enabled),
      render_pre_processor_enabled_(render_pre_processor_enabled),
      capture_analyzer_enabled_(capture_analyzer_enabled) {}

bool AudioProcessingImpl::SubmoduleStates::Update(
    bool high_pass_filter_enabled,
    bool echo_canceller_enabled,
    bool mobile_echo_controller_enabled,
    bool residual_echo_detector_enabled,
    bool noise_suppressor_enabled,
    bool adaptive_gain_controller_enabled,
    bool gain_controller2_enabled,
    bool pre_amplifier_enabled,
    bool echo_controller_enabled,
    bool voice_detector_enabled,
    bool transient_suppressor_enabled) {
  bool changed = false;
  changed |= (high_pass_filter_enabled != high_pass_filter_enabled_);
  changed |= (echo_canceller_enabled != echo_canceller_enabled_);
  changed |=
      (mobile_echo_controller_enabled != mobile_echo_controller_enabled_);
  changed |=
      (residual_echo_detector_enabled != residual_echo_detector_enabled_);
  changed |= (noise_suppressor_enabled != noise_suppressor_enabled_);
  changed |=
      (adaptive_gain_controller_enabled != adaptive_gain_controller_enabled_);
  changed |= (gain_controller2_enabled != gain_controller2_enabled_);
  changed |= (pre_amplifier_enabled_ != pre_amplifier_enabled);
  changed |= (echo_controller_enabled != echo_controller_enabled_);
  changed |= (voice_detector_enabled != voice_detector_enabled_);
  changed |= (transient_suppressor_enabled != transient_suppressor_enabled_);
  if (changed) {
    high_pass_filter_enabled_ = high_pass_filter_enabled;
    echo_canceller_enabled_ = echo_canceller_enabled;
    mobile_echo_controller_enabled_ = mobile_echo_controller_enabled;
    residual_echo_detector_enabled_ = residual_echo_detector_enabled;
    noise_suppressor_enabled_ = noise_suppressor_enabled;
    adaptive_gain_controller_enabled_ = adaptive_gain_controller_enabled;
    gain_controller2_enabled_ = gain_controller2_enabled;
    pre_amplifier_enabled_ = pre_amplifier_enabled;
    echo_controller_enabled_ = echo_controller_enabled;
    voice_detector_enabled_ = voice_detector_enabled;
    transient_suppressor_enabled_ = transient_suppressor_enabled;
  }

  changed |= first_update_;
  first_update_ = false;
  return changed;
}

bool AudioProcessingImpl::SubmoduleStates::CaptureMultiBandSubModulesActive()
    const {
  return CaptureMultiBandProcessingPresent() || voice_detector_enabled_;
}

bool AudioProcessingImpl::SubmoduleStates::CaptureMultiBandProcessingPresent()
    const {
  // If echo controller is present, assume it performs active processing.
  return CaptureMultiBandProcessingActive(/*ec_processing_active=*/true);
}

bool AudioProcessingImpl::SubmoduleStates::CaptureMultiBandProcessingActive(
    bool ec_processing_active) const {
  return high_pass_filter_enabled_ || echo_canceller_enabled_ ||
         mobile_echo_controller_enabled_ || noise_suppressor_enabled_ ||
         adaptive_gain_controller_enabled_ ||
         (echo_controller_enabled_ && ec_processing_active);
}

bool AudioProcessingImpl::SubmoduleStates::CaptureFullBandProcessingActive()
    const {
  return gain_controller2_enabled_ || capture_post_processor_enabled_ ||
         pre_amplifier_enabled_;
}

bool AudioProcessingImpl::SubmoduleStates::CaptureAnalyzerActive() const {
  return capture_analyzer_enabled_;
}

bool AudioProcessingImpl::SubmoduleStates::RenderMultiBandSubModulesActive()
    const {
  return RenderMultiBandProcessingActive() || echo_canceller_enabled_ ||
         mobile_echo_controller_enabled_ || adaptive_gain_controller_enabled_ ||
         echo_controller_enabled_;
}

bool AudioProcessingImpl::SubmoduleStates::RenderFullBandProcessingActive()
    const {
  return render_pre_processor_enabled_;
}

bool AudioProcessingImpl::SubmoduleStates::RenderMultiBandProcessingActive()
    const {
  return false;
}

bool AudioProcessingImpl::SubmoduleStates::HighPassFilteringRequired() const {
  return high_pass_filter_enabled_ || echo_canceller_enabled_ ||
         mobile_echo_controller_enabled_ || noise_suppressor_enabled_;
}

AudioProcessingBuilder::AudioProcessingBuilder() = default;
AudioProcessingBuilder::~AudioProcessingBuilder() = default;

AudioProcessingBuilder& AudioProcessingBuilder::SetCapturePostProcessing(
    std::unique_ptr<CustomProcessing> capture_post_processing) {
  capture_post_processing_ = std::move(capture_post_processing);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetRenderPreProcessing(
    std::unique_ptr<CustomProcessing> render_pre_processing) {
  render_pre_processing_ = std::move(render_pre_processing);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetCaptureAnalyzer(
    std::unique_ptr<CustomAudioAnalyzer> capture_analyzer) {
  capture_analyzer_ = std::move(capture_analyzer);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetEchoControlFactory(
    std::unique_ptr<EchoControlFactory> echo_control_factory) {
  echo_control_factory_ = std::move(echo_control_factory);
  return *this;
}

AudioProcessingBuilder& AudioProcessingBuilder::SetEchoDetector(
    rtc::scoped_refptr<EchoDetector> echo_detector) {
  echo_detector_ = std::move(echo_detector);
  return *this;
}

AudioProcessing* AudioProcessingBuilder::Create() {
  webrtc::Config config;
  return Create(config);
}

AudioProcessing* AudioProcessingBuilder::Create(const webrtc::Config& config) {
  AudioProcessingImpl* apm = new rtc::RefCountedObject<AudioProcessingImpl>(
      config, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
  if (apm->Initialize() != AudioProcessing::kNoError) {
    delete apm;
    apm = nullptr;
  }
  return apm;
}

AudioProcessingImpl::AudioProcessingImpl(const webrtc::Config& config)
    : AudioProcessingImpl(config,
                          /*capture_post_processor=*/nullptr,
                          /*render_pre_processor=*/nullptr,
                          /*echo_control_factory=*/nullptr,
                          /*echo_detector=*/nullptr,
                          /*capture_analyzer=*/nullptr) {}

int AudioProcessingImpl::instance_count_ = 0;

AudioProcessingImpl::AudioProcessingImpl(
    const webrtc::Config& config,
    std::unique_ptr<CustomProcessing> capture_post_processor,
    std::unique_ptr<CustomProcessing> render_pre_processor,
    std::unique_ptr<EchoControlFactory> echo_control_factory,
    rtc::scoped_refptr<EchoDetector> echo_detector,
    std::unique_ptr<CustomAudioAnalyzer> capture_analyzer)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      enforced_usage_of_legacy_ns_(DetectLegacyNsEnforcement()),
      use_setup_specific_default_aec3_config_(
          UseSetupSpecificDefaultAec3Congfig()),
      capture_runtime_settings_(kRuntimeSettingQueueSize),
      render_runtime_settings_(kRuntimeSettingQueueSize),
      capture_runtime_settings_enqueuer_(&capture_runtime_settings_),
      render_runtime_settings_enqueuer_(&render_runtime_settings_),
      echo_control_factory_(std::move(echo_control_factory)),
      submodule_states_(!!capture_post_processor,
                        !!render_pre_processor,
                        !!capture_analyzer),
      submodules_(std::move(capture_post_processor),
                  std::move(render_pre_processor),
                  std::move(echo_detector),
                  std::move(capture_analyzer)),
      constants_(config.Get<ExperimentalAgc>().startup_min_volume,
                 config.Get<ExperimentalAgc>().clipped_level_min,
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
                 /* enabled= */ false,
                 /* enabled_agc2_level_estimator= */ false,
                 /* digital_adaptive_disabled= */ false,
#else
                 config.Get<ExperimentalAgc>().enabled,
                 config.Get<ExperimentalAgc>().enabled_agc2_level_estimator,
                 config.Get<ExperimentalAgc>().digital_adaptive_disabled,
#endif
                 !field_trial::IsEnabled(
                     "WebRTC-ApmExperimentalMultiChannelRenderKillSwitch"),
                 !field_trial::IsEnabled(
                     "WebRTC-ApmExperimentalMultiChannelCaptureKillSwitch")),
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
      capture_(false),
#else
      capture_(config.Get<ExperimentalNs>().enabled),
#endif
      capture_nonlocked_() {
  RTC_LOG(LS_INFO) << "Injected APM submodules:"
                   << "\nEcho control factory: " << !!echo_control_factory_
                   << "\nEcho detector: " << !!submodules_.echo_detector
                   << "\nCapture analyzer: " << !!submodules_.capture_analyzer
                   << "\nCapture post processor: "
                   << !!submodules_.capture_post_processor
                   << "\nRender pre processor: "
                   << !!submodules_.render_pre_processor;

  // Mark Echo Controller enabled if a factory is injected.
  capture_nonlocked_.echo_controller_enabled =
      static_cast<bool>(echo_control_factory_);

  submodules_.gain_control.reset(new GainControlImpl());

  // If no echo detector is injected, use the ResidualEchoDetector.
  if (!submodules_.echo_detector) {
    submodules_.echo_detector =
        new rtc::RefCountedObject<ResidualEchoDetector>();
  }

  // TODO(alessiob): Move the injected gain controller once injection is
  // implemented.
  submodules_.gain_controller2.reset(new GainController2());

  SetExtraOptions(config);
}

AudioProcessingImpl::~AudioProcessingImpl() = default;

int AudioProcessingImpl::Initialize() {
  // Run in a single-threaded manner during initialization.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  return InitializeLocked();
}

int AudioProcessingImpl::Initialize(int capture_input_sample_rate_hz,
                                    int capture_output_sample_rate_hz,
                                    int render_input_sample_rate_hz,
                                    ChannelLayout capture_input_layout,
                                    ChannelLayout capture_output_layout,
                                    ChannelLayout render_input_layout) {
  const ProcessingConfig processing_config = {
      {{capture_input_sample_rate_hz, ChannelsFromLayout(capture_input_layout),
        LayoutHasKeyboard(capture_input_layout)},
       {capture_output_sample_rate_hz,
        ChannelsFromLayout(capture_output_layout),
        LayoutHasKeyboard(capture_output_layout)},
       {render_input_sample_rate_hz, ChannelsFromLayout(render_input_layout),
        LayoutHasKeyboard(render_input_layout)},
       {render_input_sample_rate_hz, ChannelsFromLayout(render_input_layout),
        LayoutHasKeyboard(render_input_layout)}}};

  return Initialize(processing_config);
}

int AudioProcessingImpl::Initialize(const ProcessingConfig& processing_config) {
  // Run in a single-threaded manner during initialization.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  return InitializeLocked(processing_config);
}

int AudioProcessingImpl::MaybeInitializeRender(
    const ProcessingConfig& processing_config) {
  // Called from both threads. Thread check is therefore not possible.
  if (processing_config == formats_.api_format) {
    return kNoError;
  }

  rtc::CritScope cs_capture(&crit_capture_);
  return InitializeLocked(processing_config);
}

int AudioProcessingImpl::InitializeLocked() {
  UpdateActiveSubmoduleStates();

  const int render_audiobuffer_sample_rate_hz =
      formats_.api_format.reverse_output_stream().num_frames() == 0
          ? formats_.render_processing_format.sample_rate_hz()
          : formats_.api_format.reverse_output_stream().sample_rate_hz();
  if (formats_.api_format.reverse_input_stream().num_channels() > 0) {
    render_.render_audio.reset(new AudioBuffer(
        formats_.api_format.reverse_input_stream().sample_rate_hz(),
        formats_.api_format.reverse_input_stream().num_channels(),
        formats_.render_processing_format.sample_rate_hz(),
        formats_.render_processing_format.num_channels(),
        render_audiobuffer_sample_rate_hz,
        formats_.render_processing_format.num_channels()));
    if (formats_.api_format.reverse_input_stream() !=
        formats_.api_format.reverse_output_stream()) {
      render_.render_converter = AudioConverter::Create(
          formats_.api_format.reverse_input_stream().num_channels(),
          formats_.api_format.reverse_input_stream().num_frames(),
          formats_.api_format.reverse_output_stream().num_channels(),
          formats_.api_format.reverse_output_stream().num_frames());
    } else {
      render_.render_converter.reset(nullptr);
    }
  } else {
    render_.render_audio.reset(nullptr);
    render_.render_converter.reset(nullptr);
  }

  capture_.capture_audio.reset(new AudioBuffer(
      formats_.api_format.input_stream().sample_rate_hz(),
      formats_.api_format.input_stream().num_channels(),
      capture_nonlocked_.capture_processing_format.sample_rate_hz(),
      formats_.api_format.output_stream().num_channels(),
      formats_.api_format.output_stream().sample_rate_hz(),
      formats_.api_format.output_stream().num_channels()));

  if (capture_nonlocked_.capture_processing_format.sample_rate_hz() <
          formats_.api_format.output_stream().sample_rate_hz() &&
      formats_.api_format.output_stream().sample_rate_hz() == 48000) {
    capture_.capture_fullband_audio.reset(
        new AudioBuffer(formats_.api_format.input_stream().sample_rate_hz(),
                        formats_.api_format.input_stream().num_channels(),
                        formats_.api_format.output_stream().sample_rate_hz(),
                        formats_.api_format.output_stream().num_channels(),
                        formats_.api_format.output_stream().sample_rate_hz(),
                        formats_.api_format.output_stream().num_channels()));
  } else {
    capture_.capture_fullband_audio.reset();
  }

  AllocateRenderQueue();

  submodules_.gain_control->Initialize(num_proc_channels(),
                                       proc_sample_rate_hz());
  if (constants_.use_experimental_agc) {
    if (!submodules_.agc_manager.get() ||
        submodules_.agc_manager->num_channels() !=
            static_cast<int>(num_proc_channels()) ||
        submodules_.agc_manager->sample_rate_hz() !=
            capture_nonlocked_.split_rate) {
      int stream_analog_level = -1;
      const bool re_creation = !!submodules_.agc_manager;
      if (re_creation) {
        stream_analog_level = submodules_.agc_manager->stream_analog_level();
      }
      submodules_.agc_manager.reset(new AgcManagerDirect(
          num_proc_channels(), constants_.agc_startup_min_volume,
          constants_.agc_clipped_level_min,
          constants_.use_experimental_agc_agc2_level_estimation,
          constants_.use_experimental_agc_agc2_digital_adaptive,
          capture_nonlocked_.split_rate));
      if (re_creation) {
        submodules_.agc_manager->set_stream_analog_level(stream_analog_level);
      }
    }
    submodules_.agc_manager->Initialize();
    submodules_.agc_manager->SetupDigitalGainControl(
        submodules_.gain_control.get());
    submodules_.agc_manager->SetCaptureMuted(capture_.output_will_be_muted);
  }
  InitializeTransient();
  InitializeHighPassFilter();
  InitializeVoiceDetector();
  InitializeResidualEchoDetector();
  InitializeEchoController();
  InitializeGainController2();
  InitializeNoiseSuppressor();
  InitializeAnalyzer();
  InitializePostProcessor();
  InitializePreProcessor();

  if (aec_dump_) {
    aec_dump_->WriteInitMessage(formats_.api_format, rtc::TimeUTCMillis());
  }
  return kNoError;
}

int AudioProcessingImpl::InitializeLocked(const ProcessingConfig& config) {
  UpdateActiveSubmoduleStates();

  for (const auto& stream : config.streams) {
    if (stream.num_channels() > 0 && stream.sample_rate_hz() <= 0) {
      return kBadSampleRateError;
    }
  }

  const size_t num_in_channels = config.input_stream().num_channels();
  const size_t num_out_channels = config.output_stream().num_channels();

  // Need at least one input channel.
  // Need either one output channel or as many outputs as there are inputs.
  if (num_in_channels == 0 ||
      !(num_out_channels == 1 || num_out_channels == num_in_channels)) {
    return kBadNumberChannelsError;
  }

  formats_.api_format = config;

  // Choose maximum rate to use for the split filtering.
  RTC_DCHECK(config_.pipeline.maximum_internal_processing_rate == 48000 ||
             config_.pipeline.maximum_internal_processing_rate == 32000);
  int max_splitting_rate = 48000;
  if (config_.pipeline.maximum_internal_processing_rate == 32000) {
    max_splitting_rate = config_.pipeline.maximum_internal_processing_rate;
  }

  int capture_processing_rate = SuitableProcessRate(
      std::min(formats_.api_format.input_stream().sample_rate_hz(),
               formats_.api_format.output_stream().sample_rate_hz()),
      max_splitting_rate,
      submodule_states_.CaptureMultiBandSubModulesActive() ||
          submodule_states_.RenderMultiBandSubModulesActive());
  RTC_DCHECK_NE(8000, capture_processing_rate);

  capture_nonlocked_.capture_processing_format =
      StreamConfig(capture_processing_rate);

  int render_processing_rate;
  if (!capture_nonlocked_.echo_controller_enabled) {
    render_processing_rate = SuitableProcessRate(
        std::min(formats_.api_format.reverse_input_stream().sample_rate_hz(),
                 formats_.api_format.reverse_output_stream().sample_rate_hz()),
        max_splitting_rate,
        submodule_states_.CaptureMultiBandSubModulesActive() ||
            submodule_states_.RenderMultiBandSubModulesActive());
  } else {
    render_processing_rate = capture_processing_rate;
  }

  // If the forward sample rate is 8 kHz, the render stream is also processed
  // at this rate.
  if (capture_nonlocked_.capture_processing_format.sample_rate_hz() ==
      kSampleRate8kHz) {
    render_processing_rate = kSampleRate8kHz;
  } else {
    render_processing_rate =
        std::max(render_processing_rate, static_cast<int>(kSampleRate16kHz));
  }

  RTC_DCHECK_NE(8000, render_processing_rate);

  if (submodule_states_.RenderMultiBandSubModulesActive()) {
    // By default, downmix the render stream to mono for analysis. This has been
    // demonstrated to work well for AEC in most practical scenarios.
    const bool multi_channel_render = config_.pipeline.multi_channel_render &&
                                      constants_.multi_channel_render_support;
    int render_processing_num_channels =
        multi_channel_render
            ? formats_.api_format.reverse_input_stream().num_channels()
            : 1;
    formats_.render_processing_format =
        StreamConfig(render_processing_rate, render_processing_num_channels);
  } else {
    formats_.render_processing_format = StreamConfig(
        formats_.api_format.reverse_input_stream().sample_rate_hz(),
        formats_.api_format.reverse_input_stream().num_channels());
  }

  if (capture_nonlocked_.capture_processing_format.sample_rate_hz() ==
          kSampleRate32kHz ||
      capture_nonlocked_.capture_processing_format.sample_rate_hz() ==
          kSampleRate48kHz) {
    capture_nonlocked_.split_rate = kSampleRate16kHz;
  } else {
    capture_nonlocked_.split_rate =
        capture_nonlocked_.capture_processing_format.sample_rate_hz();
  }

  return InitializeLocked();
}

void AudioProcessingImpl::ApplyConfig(const AudioProcessing::Config& config) {
  RTC_LOG(LS_INFO) << "AudioProcessing::ApplyConfig: " << config.ToString();

  // Run in a single-threaded manner when applying the settings.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

  const bool pipeline_config_changed =
      config_.pipeline.multi_channel_render !=
          config.pipeline.multi_channel_render ||
      config_.pipeline.multi_channel_capture !=
          config.pipeline.multi_channel_capture;

  const bool aec_config_changed =
      config_.echo_canceller.enabled != config.echo_canceller.enabled ||
      config_.echo_canceller.use_legacy_aec !=
          config.echo_canceller.use_legacy_aec ||
      config_.echo_canceller.mobile_mode != config.echo_canceller.mobile_mode ||
      (config_.echo_canceller.enabled && config.echo_canceller.use_legacy_aec &&
       config_.echo_canceller.legacy_moderate_suppression_level !=
           config.echo_canceller.legacy_moderate_suppression_level);

  const bool agc1_config_changed =
      config_.gain_controller1.enabled != config.gain_controller1.enabled ||
      config_.gain_controller1.mode != config.gain_controller1.mode ||
      config_.gain_controller1.target_level_dbfs !=
          config.gain_controller1.target_level_dbfs ||
      config_.gain_controller1.compression_gain_db !=
          config.gain_controller1.compression_gain_db ||
      config_.gain_controller1.enable_limiter !=
          config.gain_controller1.enable_limiter ||
      config_.gain_controller1.analog_level_minimum !=
          config.gain_controller1.analog_level_minimum ||
      config_.gain_controller1.analog_level_maximum !=
          config.gain_controller1.analog_level_maximum;

  const bool voice_detection_config_changed =
      config_.voice_detection.enabled != config.voice_detection.enabled;

  const bool ns_config_changed =
      config_.noise_suppression.enabled != config.noise_suppression.enabled ||
      config_.noise_suppression.level != config.noise_suppression.level;

  config_ = config;

  if (aec_config_changed) {
    InitializeEchoController();
  }

  if (ns_config_changed) {
    InitializeNoiseSuppressor();
  }

  InitializeHighPassFilter();

  if (agc1_config_changed) {
    ApplyAgc1Config(config_.gain_controller1);
  }

  const bool config_ok = GainController2::Validate(config_.gain_controller2);
  if (!config_ok) {
    RTC_LOG(LS_ERROR) << "AudioProcessing module config error\n"
                         "Gain Controller 2: "
                      << GainController2::ToString(config_.gain_controller2)
                      << "\nReverting to default parameter set";
    config_.gain_controller2 = AudioProcessing::Config::GainController2();
  }
  InitializeGainController2();
  InitializePreAmplifier();
  submodules_.gain_controller2->ApplyConfig(config_.gain_controller2);

  if (config_.level_estimation.enabled && !submodules_.output_level_estimator) {
    submodules_.output_level_estimator = std::make_unique<LevelEstimator>();
  }

  if (voice_detection_config_changed) {
    InitializeVoiceDetector();
  }

  // Reinitialization must happen after all submodule configuration to avoid
  // additional reinitializations on the next capture / render processing call.
  if (pipeline_config_changed) {
    InitializeLocked(formats_.api_format);
  }
}

void AudioProcessingImpl::ApplyAgc1Config(
    const Config::GainController1& config) {
  int error = submodules_.gain_control->Enable(config.enabled);
  RTC_DCHECK_EQ(kNoError, error);

  if (!submodules_.agc_manager) {
    error = submodules_.gain_control->set_mode(
        Agc1ConfigModeToInterfaceMode(config.mode));
    RTC_DCHECK_EQ(kNoError, error);
    error = submodules_.gain_control->set_target_level_dbfs(
        config.target_level_dbfs);
    RTC_DCHECK_EQ(kNoError, error);
    error = submodules_.gain_control->set_compression_gain_db(
        config.compression_gain_db);
    RTC_DCHECK_EQ(kNoError, error);
    error = submodules_.gain_control->enable_limiter(config.enable_limiter);
    RTC_DCHECK_EQ(kNoError, error);
    error = submodules_.gain_control->set_analog_level_limits(
        config.analog_level_minimum, config.analog_level_maximum);
    RTC_DCHECK_EQ(kNoError, error);
  }
}

void AudioProcessingImpl::SetExtraOptions(const webrtc::Config& config) {
  // Run in a single-threaded manner when setting the extra options.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

  capture_nonlocked_.use_aec2_extended_filter =
      config.Get<ExtendedFilter>().enabled;
  capture_nonlocked_.use_aec2_delay_agnostic =
      config.Get<DelayAgnostic>().enabled;
  capture_nonlocked_.use_aec2_refined_adaptive_filter =
      config.Get<RefinedAdaptiveFilter>().enabled;

  if (capture_.transient_suppressor_enabled !=
      config.Get<ExperimentalNs>().enabled) {
    capture_.transient_suppressor_enabled =
        config.Get<ExperimentalNs>().enabled;
    InitializeTransient();
  }
}

int AudioProcessingImpl::proc_sample_rate_hz() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_nonlocked_.capture_processing_format.sample_rate_hz();
}

int AudioProcessingImpl::proc_fullband_sample_rate_hz() const {
  return capture_.capture_fullband_audio
             ? capture_.capture_fullband_audio->num_frames() * 100
             : capture_nonlocked_.capture_processing_format.sample_rate_hz();
}

int AudioProcessingImpl::proc_split_sample_rate_hz() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_nonlocked_.split_rate;
}

size_t AudioProcessingImpl::num_reverse_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  return formats_.render_processing_format.num_channels();
}

size_t AudioProcessingImpl::num_input_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  return formats_.api_format.input_stream().num_channels();
}

size_t AudioProcessingImpl::num_proc_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  const bool multi_channel_capture = config_.pipeline.multi_channel_capture &&
                                     constants_.multi_channel_capture_support;
  if (capture_nonlocked_.echo_controller_enabled && !multi_channel_capture) {
    return 1;
  }
  return num_output_channels();
}

size_t AudioProcessingImpl::num_output_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  return formats_.api_format.output_stream().num_channels();
}

void AudioProcessingImpl::set_output_will_be_muted(bool muted) {
  rtc::CritScope cs(&crit_capture_);
  capture_.output_will_be_muted = muted;
  if (submodules_.agc_manager.get()) {
    submodules_.agc_manager->SetCaptureMuted(capture_.output_will_be_muted);
  }
}

void AudioProcessingImpl::SetRuntimeSetting(RuntimeSetting setting) {
  switch (setting.type()) {
    case RuntimeSetting::Type::kCustomRenderProcessingRuntimeSetting:
    case RuntimeSetting::Type::kPlayoutAudioDeviceChange:
      render_runtime_settings_enqueuer_.Enqueue(setting);
      return;
    case RuntimeSetting::Type::kCapturePreGain:
    case RuntimeSetting::Type::kCaptureCompressionGain:
    case RuntimeSetting::Type::kCaptureFixedPostGain:
      capture_runtime_settings_enqueuer_.Enqueue(setting);
      return;
    case RuntimeSetting::Type::kPlayoutVolumeChange:
      capture_runtime_settings_enqueuer_.Enqueue(setting);
      render_runtime_settings_enqueuer_.Enqueue(setting);
      return;
    case RuntimeSetting::Type::kNotSpecified:
      RTC_NOTREACHED();
      return;
  }
  // The language allows the enum to have a non-enumerator
  // value. Check that this doesn't happen.
  RTC_NOTREACHED();
}

AudioProcessingImpl::RuntimeSettingEnqueuer::RuntimeSettingEnqueuer(
    SwapQueue<RuntimeSetting>* runtime_settings)
    : runtime_settings_(*runtime_settings) {
  RTC_DCHECK(runtime_settings);
}

AudioProcessingImpl::RuntimeSettingEnqueuer::~RuntimeSettingEnqueuer() =
    default;

void AudioProcessingImpl::RuntimeSettingEnqueuer::Enqueue(
    RuntimeSetting setting) {
  size_t remaining_attempts = 10;
  while (!runtime_settings_.Insert(&setting) && remaining_attempts-- > 0) {
    RuntimeSetting setting_to_discard;
    if (runtime_settings_.Remove(&setting_to_discard))
      RTC_LOG(LS_ERROR)
          << "The runtime settings queue is full. Oldest setting discarded.";
  }
  if (remaining_attempts == 0)
    RTC_LOG(LS_ERROR) << "Cannot enqueue a new runtime setting.";
}

int AudioProcessingImpl::ProcessStream(const float* const* src,
                                       const StreamConfig& input_config,
                                       const StreamConfig& output_config,
                                       float* const* dest) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessStream_StreamConfig");
  ProcessingConfig processing_config;
  bool reinitialization_required = false;
  {
    // Acquire the capture lock in order to safely call the function
    // that retrieves the render side data. This function accesses apm
    // getters that need the capture lock held when being called.
    rtc::CritScope cs_capture(&crit_capture_);
    EmptyQueuedRenderAudio();

    if (!src || !dest) {
      return kNullPointerError;
    }

    processing_config = formats_.api_format;
    reinitialization_required = UpdateActiveSubmoduleStates();
  }

  if (processing_config.input_stream() != input_config) {
    processing_config.input_stream() = input_config;
    reinitialization_required = true;
  }

  if (processing_config.output_stream() != output_config) {
    processing_config.output_stream() = output_config;
    reinitialization_required = true;
  }

  if (reinitialization_required) {
    // Reinitialize.
    rtc::CritScope cs_render(&crit_render_);
    rtc::CritScope cs_capture(&crit_capture_);
    RETURN_ON_ERR(InitializeLocked(processing_config));
  }

  rtc::CritScope cs_capture(&crit_capture_);
  RTC_DCHECK_EQ(processing_config.input_stream().num_frames(),
                formats_.api_format.input_stream().num_frames());

  if (aec_dump_) {
    RecordUnprocessedCaptureStream(src);
  }

  capture_.keyboard_info.Extract(src, formats_.api_format.input_stream());
  capture_.capture_audio->CopyFrom(src, formats_.api_format.input_stream());
  if (capture_.capture_fullband_audio) {
    capture_.capture_fullband_audio->CopyFrom(
        src, formats_.api_format.input_stream());
  }
  RETURN_ON_ERR(ProcessCaptureStreamLocked());
  if (capture_.capture_fullband_audio) {
    capture_.capture_fullband_audio->CopyTo(formats_.api_format.output_stream(),
                                            dest);
  } else {
    capture_.capture_audio->CopyTo(formats_.api_format.output_stream(), dest);
  }

  if (aec_dump_) {
    RecordProcessedCaptureStream(dest);
  }
  return kNoError;
}

void AudioProcessingImpl::HandleCaptureRuntimeSettings() {
  RuntimeSetting setting;
  while (capture_runtime_settings_.Remove(&setting)) {
    if (aec_dump_) {
      aec_dump_->WriteRuntimeSetting(setting);
    }
    switch (setting.type()) {
      case RuntimeSetting::Type::kCapturePreGain:
        if (config_.pre_amplifier.enabled) {
          float value;
          setting.GetFloat(&value);
          config_.pre_amplifier.fixed_gain_factor = value;
          submodules_.pre_amplifier->SetGainFactor(value);
        }
        // TODO(bugs.chromium.org/9138): Log setting handling by Aec Dump.
        break;
      case RuntimeSetting::Type::kCaptureCompressionGain: {
        if (!submodules_.agc_manager) {
          float value;
          setting.GetFloat(&value);
          int int_value = static_cast<int>(value + .5f);
          config_.gain_controller1.compression_gain_db = int_value;
          int error =
              submodules_.gain_control->set_compression_gain_db(int_value);
          RTC_DCHECK_EQ(kNoError, error);
        }
        break;
      }
      case RuntimeSetting::Type::kCaptureFixedPostGain: {
        if (config_.gain_controller2.enabled) {
          float value;
          setting.GetFloat(&value);
          config_.gain_controller2.fixed_digital.gain_db = value;
          submodules_.gain_controller2->ApplyConfig(config_.gain_controller2);
        }
        break;
      }
      case RuntimeSetting::Type::kPlayoutVolumeChange: {
        int value;
        setting.GetInt(&value);
        capture_.playout_volume = value;
        break;
      }
      case RuntimeSetting::Type::kPlayoutAudioDeviceChange:
        RTC_NOTREACHED();
        break;
      case RuntimeSetting::Type::kCustomRenderProcessingRuntimeSetting:
        RTC_NOTREACHED();
        break;
      case RuntimeSetting::Type::kNotSpecified:
        RTC_NOTREACHED();
        break;
    }
  }
}

void AudioProcessingImpl::HandleRenderRuntimeSettings() {
  RuntimeSetting setting;
  while (render_runtime_settings_.Remove(&setting)) {
    if (aec_dump_) {
      aec_dump_->WriteRuntimeSetting(setting);
    }
    switch (setting.type()) {
      case RuntimeSetting::Type::kPlayoutAudioDeviceChange:  // fall-through
      case RuntimeSetting::Type::kPlayoutVolumeChange:       // fall-through
      case RuntimeSetting::Type::kCustomRenderProcessingRuntimeSetting:
        if (submodules_.render_pre_processor) {
          submodules_.render_pre_processor->SetRuntimeSetting(setting);
        }
        break;
      case RuntimeSetting::Type::kCapturePreGain:          // fall-through
      case RuntimeSetting::Type::kCaptureCompressionGain:  // fall-through
      case RuntimeSetting::Type::kCaptureFixedPostGain:    // fall-through
      case RuntimeSetting::Type::kNotSpecified:
        RTC_NOTREACHED();
        break;
    }
  }
}

void AudioProcessingImpl::QueueBandedRenderAudio(AudioBuffer* audio) {
  RTC_DCHECK_GE(160, audio->num_frames_per_band());

  // Insert the samples into the queue.
  if (submodules_.echo_cancellation) {
    RTC_DCHECK(aec_render_signal_queue_);
    EchoCancellationImpl::PackRenderAudioBuffer(audio, num_output_channels(),
                                                num_reverse_channels(),
                                                &aec_render_queue_buffer_);

    if (!aec_render_signal_queue_->Insert(&aec_render_queue_buffer_)) {
      // The data queue is full and needs to be emptied.
      EmptyQueuedRenderAudio();

      // Retry the insert (should always work).
      bool result = aec_render_signal_queue_->Insert(&aec_render_queue_buffer_);
      RTC_DCHECK(result);
    }
  }

  if (submodules_.echo_control_mobile) {
    EchoControlMobileImpl::PackRenderAudioBuffer(audio, num_output_channels(),
                                                 num_reverse_channels(),
                                                 &aecm_render_queue_buffer_);
    RTC_DCHECK(aecm_render_signal_queue_);
    // Insert the samples into the queue.
    if (!aecm_render_signal_queue_->Insert(&aecm_render_queue_buffer_)) {
      // The data queue is full and needs to be emptied.
      EmptyQueuedRenderAudio();

      // Retry the insert (should always work).
      bool result =
          aecm_render_signal_queue_->Insert(&aecm_render_queue_buffer_);
      RTC_DCHECK(result);
    }
  }

  if (!submodules_.agc_manager) {
    GainControlImpl::PackRenderAudioBuffer(*audio, &agc_render_queue_buffer_);
    // Insert the samples into the queue.
    if (!agc_render_signal_queue_->Insert(&agc_render_queue_buffer_)) {
      // The data queue is full and needs to be emptied.
      EmptyQueuedRenderAudio();

      // Retry the insert (should always work).
      bool result = agc_render_signal_queue_->Insert(&agc_render_queue_buffer_);
      RTC_DCHECK(result);
    }
  }
}

void AudioProcessingImpl::QueueNonbandedRenderAudio(AudioBuffer* audio) {
  ResidualEchoDetector::PackRenderAudioBuffer(audio, &red_render_queue_buffer_);

  // Insert the samples into the queue.
  if (!red_render_signal_queue_->Insert(&red_render_queue_buffer_)) {
    // The data queue is full and needs to be emptied.
    EmptyQueuedRenderAudio();

    // Retry the insert (should always work).
    bool result = red_render_signal_queue_->Insert(&red_render_queue_buffer_);
    RTC_DCHECK(result);
  }
}

void AudioProcessingImpl::AllocateRenderQueue() {
  const size_t new_agc_render_queue_element_max_size =
      std::max(static_cast<size_t>(1), kMaxAllowedValuesOfSamplesPerBand);

  const size_t new_red_render_queue_element_max_size =
      std::max(static_cast<size_t>(1), kMaxAllowedValuesOfSamplesPerFrame);

  // Reallocate the queues if the queue item sizes are too small to fit the
  // data to put in the queues.

  if (agc_render_queue_element_max_size_ <
      new_agc_render_queue_element_max_size) {
    agc_render_queue_element_max_size_ = new_agc_render_queue_element_max_size;

    std::vector<int16_t> template_queue_element(
        agc_render_queue_element_max_size_);

    agc_render_signal_queue_.reset(
        new SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<int16_t>(
                agc_render_queue_element_max_size_)));

    agc_render_queue_buffer_.resize(agc_render_queue_element_max_size_);
    agc_capture_queue_buffer_.resize(agc_render_queue_element_max_size_);
  } else {
    agc_render_signal_queue_->Clear();
  }

  if (red_render_queue_element_max_size_ <
      new_red_render_queue_element_max_size) {
    red_render_queue_element_max_size_ = new_red_render_queue_element_max_size;

    std::vector<float> template_queue_element(
        red_render_queue_element_max_size_);

    red_render_signal_queue_.reset(
        new SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<float>(
                red_render_queue_element_max_size_)));

    red_render_queue_buffer_.resize(red_render_queue_element_max_size_);
    red_capture_queue_buffer_.resize(red_render_queue_element_max_size_);
  } else {
    red_render_signal_queue_->Clear();
  }
}

void AudioProcessingImpl::EmptyQueuedRenderAudio() {
  rtc::CritScope cs_capture(&crit_capture_);
  if (submodules_.echo_cancellation) {
    RTC_DCHECK(aec_render_signal_queue_);
    while (aec_render_signal_queue_->Remove(&aec_capture_queue_buffer_)) {
      submodules_.echo_cancellation->ProcessRenderAudio(
          aec_capture_queue_buffer_);
    }
  }

  if (submodules_.echo_control_mobile) {
    RTC_DCHECK(aecm_render_signal_queue_);
    while (aecm_render_signal_queue_->Remove(&aecm_capture_queue_buffer_)) {
      submodules_.echo_control_mobile->ProcessRenderAudio(
          aecm_capture_queue_buffer_);
    }
  }

  while (agc_render_signal_queue_->Remove(&agc_capture_queue_buffer_)) {
    submodules_.gain_control->ProcessRenderAudio(agc_capture_queue_buffer_);
  }

  while (red_render_signal_queue_->Remove(&red_capture_queue_buffer_)) {
    RTC_DCHECK(submodules_.echo_detector);
    submodules_.echo_detector->AnalyzeRenderAudio(red_capture_queue_buffer_);
  }
}

int AudioProcessingImpl::ProcessStream(AudioFrame* frame) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessStream_AudioFrame");
  {
    // Acquire the capture lock in order to safely call the function
    // that retrieves the render side data. This function accesses APM
    // getters that need the capture lock held when being called.
    rtc::CritScope cs_capture(&crit_capture_);
    EmptyQueuedRenderAudio();
  }

  if (!frame) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz &&
      frame->sample_rate_hz_ != kSampleRate48kHz) {
    return kBadSampleRateError;
  }

  ProcessingConfig processing_config;
  bool reinitialization_required = false;
  {
    // Aquire lock for the access of api_format.
    // The lock is released immediately due to the conditional
    // reinitialization.
    rtc::CritScope cs_capture(&crit_capture_);
    // TODO(ajm): The input and output rates and channels are currently
    // constrained to be identical in the int16 interface.
    processing_config = formats_.api_format;

    reinitialization_required = UpdateActiveSubmoduleStates();
  }

  reinitialization_required =
      reinitialization_required ||
      processing_config.input_stream().sample_rate_hz() !=
          frame->sample_rate_hz_ ||
      processing_config.input_stream().num_channels() != frame->num_channels_ ||
      processing_config.output_stream().sample_rate_hz() !=
          frame->sample_rate_hz_ ||
      processing_config.output_stream().num_channels() != frame->num_channels_;

  if (reinitialization_required) {
    processing_config.input_stream().set_sample_rate_hz(frame->sample_rate_hz_);
    processing_config.input_stream().set_num_channels(frame->num_channels_);
    processing_config.output_stream().set_sample_rate_hz(
        frame->sample_rate_hz_);
    processing_config.output_stream().set_num_channels(frame->num_channels_);

    // Reinitialize.
    rtc::CritScope cs_render(&crit_render_);
    rtc::CritScope cs_capture(&crit_capture_);
    RETURN_ON_ERR(InitializeLocked(processing_config));
  }

  rtc::CritScope cs_capture(&crit_capture_);
  if (frame->samples_per_channel_ !=
      formats_.api_format.input_stream().num_frames()) {
    return kBadDataLengthError;
  }

  if (aec_dump_) {
    RecordUnprocessedCaptureStream(*frame);
  }

  capture_.capture_audio->CopyFrom(frame);
  if (capture_.capture_fullband_audio) {
    capture_.capture_fullband_audio->CopyFrom(frame);
  }
  RETURN_ON_ERR(ProcessCaptureStreamLocked());
  if (submodule_states_.CaptureMultiBandProcessingPresent() ||
      submodule_states_.CaptureFullBandProcessingActive()) {
    if (capture_.capture_fullband_audio) {
      capture_.capture_fullband_audio->CopyTo(frame);
    } else {
      capture_.capture_audio->CopyTo(frame);
    }
  }
  if (capture_.stats.voice_detected) {
    frame->vad_activity_ = *capture_.stats.voice_detected
                               ? AudioFrame::kVadActive
                               : AudioFrame::kVadPassive;
  }

  if (aec_dump_) {
    RecordProcessedCaptureStream(*frame);
  }

  return kNoError;
}

int AudioProcessingImpl::ProcessCaptureStreamLocked() {
  HandleCaptureRuntimeSettings();

  // Ensure that not both the AEC and AECM are active at the same time.
  // TODO(peah): Simplify once the public API Enable functions for these
  // are moved to APM.
  RTC_DCHECK_LE(!!submodules_.echo_controller +
                    !!submodules_.echo_cancellation +
                    !!submodules_.echo_control_mobile,
                1);

  AudioBuffer* capture_buffer = capture_.capture_audio.get();  // For brevity.

  if (submodules_.pre_amplifier) {
    submodules_.pre_amplifier->ApplyGain(AudioFrameView<float>(
        capture_buffer->channels(), capture_buffer->num_channels(),
        capture_buffer->num_frames()));
  }

  capture_input_rms_.Analyze(rtc::ArrayView<const float>(
      capture_buffer->channels_const()[0],
      capture_nonlocked_.capture_processing_format.num_frames()));
  const bool log_rms = ++capture_rms_interval_counter_ >= 1000;
  if (log_rms) {
    capture_rms_interval_counter_ = 0;
    RmsLevel::Levels levels = capture_input_rms_.AverageAndPeak();
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureInputLevelAverageRms",
                                levels.average, 1, RmsLevel::kMinLevelDb, 64);
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureInputLevelPeakRms",
                                levels.peak, 1, RmsLevel::kMinLevelDb, 64);
  }

  if (submodules_.echo_controller) {
    // Detect and flag any change in the analog gain.
    int analog_mic_level = recommended_stream_analog_level();
    capture_.echo_path_gain_change =
        capture_.prev_analog_mic_level != analog_mic_level &&
        capture_.prev_analog_mic_level != -1;
    capture_.prev_analog_mic_level = analog_mic_level;

    // Detect and flag any change in the pre-amplifier gain.
    if (submodules_.pre_amplifier) {
      float pre_amp_gain = submodules_.pre_amplifier->GetGainFactor();
      capture_.echo_path_gain_change =
          capture_.echo_path_gain_change ||
          (capture_.prev_pre_amp_gain != pre_amp_gain &&
           capture_.prev_pre_amp_gain >= 0.f);
      capture_.prev_pre_amp_gain = pre_amp_gain;
    }

    // Detect volume change.
    capture_.echo_path_gain_change =
        capture_.echo_path_gain_change ||
        (capture_.prev_playout_volume != capture_.playout_volume &&
         capture_.prev_playout_volume >= 0);
    capture_.prev_playout_volume = capture_.playout_volume;

    submodules_.echo_controller->AnalyzeCapture(capture_buffer);
  }

  if (constants_.use_experimental_agc &&
      submodules_.gain_control->is_enabled()) {
    submodules_.agc_manager->AnalyzePreProcess(capture_buffer);
  }

  if (submodule_states_.CaptureMultiBandSubModulesActive() &&
      SampleRateSupportsMultiBand(
          capture_nonlocked_.capture_processing_format.sample_rate_hz())) {
    capture_buffer->SplitIntoFrequencyBands();
  }

  const bool multi_channel_capture = config_.pipeline.multi_channel_capture &&
                                     constants_.multi_channel_capture_support;
  if (submodules_.echo_controller && !multi_channel_capture) {
    // Force down-mixing of the number of channels after the detection of
    // capture signal saturation.
    // TODO(peah): Look into ensuring that this kind of tampering with the
    // AudioBuffer functionality should not be needed.
    capture_buffer->set_num_channels(1);
  }

  if (submodules_.high_pass_filter) {
    submodules_.high_pass_filter->Process(capture_buffer);
  }

  RETURN_ON_ERR(submodules_.gain_control->AnalyzeCaptureAudio(*capture_buffer));
  RTC_DCHECK(
      !(submodules_.legacy_noise_suppressor && submodules_.noise_suppressor));
  if (submodules_.noise_suppressor) {
    submodules_.noise_suppressor->Analyze(*capture_buffer);
  } else if (submodules_.legacy_noise_suppressor) {
    submodules_.legacy_noise_suppressor->AnalyzeCaptureAudio(capture_buffer);
  }

  if (submodules_.echo_control_mobile) {
    // Ensure that the stream delay was set before the call to the
    // AECM ProcessCaptureAudio function.
    if (!was_stream_delay_set()) {
      return AudioProcessing::kStreamParameterNotSetError;
    }

    if (submodules_.noise_suppressor) {
      submodules_.noise_suppressor->Process(capture_buffer);
    } else if (submodules_.legacy_noise_suppressor) {
      submodules_.echo_control_mobile->CopyLowPassReference(capture_buffer);
      submodules_.legacy_noise_suppressor->ProcessCaptureAudio(capture_buffer);
    }

    RETURN_ON_ERR(submodules_.echo_control_mobile->ProcessCaptureAudio(
        capture_buffer, stream_delay_ms()));
  } else {
    if (submodules_.echo_controller) {
      data_dumper_->DumpRaw("stream_delay", stream_delay_ms());

      if (was_stream_delay_set()) {
        submodules_.echo_controller->SetAudioBufferDelay(stream_delay_ms());
      }

      AudioBuffer* linear_aec_buffer = capture_.linear_aec_output.get();
      submodules_.echo_controller->ProcessCapture(
          capture_buffer, linear_aec_buffer, capture_.echo_path_gain_change);
    } else if (submodules_.echo_cancellation) {
      // Ensure that the stream delay was set before the call to the
      // AEC ProcessCaptureAudio function.
      if (!was_stream_delay_set()) {
        return AudioProcessing::kStreamParameterNotSetError;
      }

      RETURN_ON_ERR(submodules_.echo_cancellation->ProcessCaptureAudio(
          capture_buffer, stream_delay_ms()));
    }

    if (submodules_.noise_suppressor) {
      submodules_.noise_suppressor->Process(capture_buffer);
    } else if (submodules_.legacy_noise_suppressor) {
      submodules_.legacy_noise_suppressor->ProcessCaptureAudio(capture_buffer);
    }
  }

  if (config_.voice_detection.enabled) {
    capture_.stats.voice_detected =
        submodules_.voice_detector->ProcessCaptureAudio(capture_buffer);
  } else {
    capture_.stats.voice_detected = absl::nullopt;
  }

  if (constants_.use_experimental_agc &&
      submodules_.gain_control->is_enabled()) {
    submodules_.agc_manager->Process(capture_buffer);

    absl::optional<int> new_digital_gain =
        submodules_.agc_manager->GetDigitalComressionGain();
    if (new_digital_gain) {
      submodules_.gain_control->set_compression_gain_db(*new_digital_gain);
    }
  }
  // TODO(peah): Add reporting from AEC3 whether there is echo.
  RETURN_ON_ERR(submodules_.gain_control->ProcessCaptureAudio(
      capture_buffer, submodules_.echo_cancellation &&
                          submodules_.echo_cancellation->stream_has_echo()));

  if (submodule_states_.CaptureMultiBandProcessingPresent() &&
      SampleRateSupportsMultiBand(
          capture_nonlocked_.capture_processing_format.sample_rate_hz())) {
    capture_buffer->MergeFrequencyBands();
  }

  if (capture_.capture_fullband_audio) {
    const auto& ec = submodules_.echo_controller;
    bool ec_active = ec ? ec->ActiveProcessing() : false;
    // Only update the fullband buffer if the multiband processing has changed
    // the signal. Keep the original signal otherwise.
    if (submodule_states_.CaptureMultiBandProcessingActive(ec_active)) {
      capture_buffer->CopyTo(capture_.capture_fullband_audio.get());
    }
    capture_buffer = capture_.capture_fullband_audio.get();
  }

  if (config_.residual_echo_detector.enabled) {
    RTC_DCHECK(submodules_.echo_detector);
    submodules_.echo_detector->AnalyzeCaptureAudio(rtc::ArrayView<const float>(
        capture_buffer->channels()[0], capture_buffer->num_frames()));
  }

  // TODO(aluebs): Investigate if the transient suppression placement should be
  // before or after the AGC.
  if (capture_.transient_suppressor_enabled) {
    float voice_probability = submodules_.agc_manager.get()
                                  ? submodules_.agc_manager->voice_probability()
                                  : 1.f;

    submodules_.transient_suppressor->Suppress(
        capture_buffer->channels()[0], capture_buffer->num_frames(),
        capture_buffer->num_channels(),
        capture_buffer->split_bands_const(0)[kBand0To8kHz],
        capture_buffer->num_frames_per_band(),
        capture_.keyboard_info.keyboard_data,
        capture_.keyboard_info.num_keyboard_frames, voice_probability,
        capture_.key_pressed);
  }

  // Experimental APM sub-module that analyzes |capture_buffer|.
  if (submodules_.capture_analyzer) {
    submodules_.capture_analyzer->Analyze(capture_buffer);
  }

  if (config_.gain_controller2.enabled) {
    submodules_.gain_controller2->NotifyAnalogLevel(
        recommended_stream_analog_level());
    submodules_.gain_controller2->Process(capture_buffer);
  }

  if (submodules_.capture_post_processor) {
    submodules_.capture_post_processor->Process(capture_buffer);
  }

  // The level estimator operates on the recombined data.
  if (config_.level_estimation.enabled) {
    submodules_.output_level_estimator->ProcessStream(*capture_buffer);
    capture_.stats.output_rms_dbfs = submodules_.output_level_estimator->RMS();
  } else {
    capture_.stats.output_rms_dbfs = absl::nullopt;
  }

  capture_output_rms_.Analyze(rtc::ArrayView<const float>(
      capture_buffer->channels_const()[0],
      capture_nonlocked_.capture_processing_format.num_frames()));
  if (log_rms) {
    RmsLevel::Levels levels = capture_output_rms_.AverageAndPeak();
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureOutputLevelAverageRms",
                                levels.average, 1, RmsLevel::kMinLevelDb, 64);
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureOutputLevelPeakRms",
                                levels.peak, 1, RmsLevel::kMinLevelDb, 64);
  }

  if (submodules_.agc_manager) {
    int level = recommended_stream_analog_level();
    data_dumper_->DumpRaw("experimental_gain_control_stream_analog_level", 1,
                          &level);
  }

  capture_.was_stream_delay_set = false;
  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStream(
    const float* const* data,
    const StreamConfig& reverse_config) {
  TRACE_EVENT0("webrtc", "AudioProcessing::AnalyzeReverseStream_StreamConfig");
  rtc::CritScope cs(&crit_render_);
  return AnalyzeReverseStreamLocked(data, reverse_config, reverse_config);
}

int AudioProcessingImpl::ProcessReverseStream(const float* const* src,
                                              const StreamConfig& input_config,
                                              const StreamConfig& output_config,
                                              float* const* dest) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessReverseStream_StreamConfig");
  rtc::CritScope cs(&crit_render_);
  RETURN_ON_ERR(AnalyzeReverseStreamLocked(src, input_config, output_config));
  if (submodule_states_.RenderMultiBandProcessingActive() ||
      submodule_states_.RenderFullBandProcessingActive()) {
    render_.render_audio->CopyTo(formats_.api_format.reverse_output_stream(),
                                 dest);
  } else if (formats_.api_format.reverse_input_stream() !=
             formats_.api_format.reverse_output_stream()) {
    render_.render_converter->Convert(src, input_config.num_samples(), dest,
                                      output_config.num_samples());
  } else {
    CopyAudioIfNeeded(src, input_config.num_frames(),
                      input_config.num_channels(), dest);
  }

  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStreamLocked(
    const float* const* src,
    const StreamConfig& input_config,
    const StreamConfig& output_config) {
  if (src == nullptr) {
    return kNullPointerError;
  }

  if (input_config.num_channels() == 0) {
    return kBadNumberChannelsError;
  }

  ProcessingConfig processing_config = formats_.api_format;
  processing_config.reverse_input_stream() = input_config;
  processing_config.reverse_output_stream() = output_config;

  RETURN_ON_ERR(MaybeInitializeRender(processing_config));
  RTC_DCHECK_EQ(input_config.num_frames(),
                formats_.api_format.reverse_input_stream().num_frames());

  if (aec_dump_) {
    const size_t channel_size =
        formats_.api_format.reverse_input_stream().num_frames();
    const size_t num_channels =
        formats_.api_format.reverse_input_stream().num_channels();
    aec_dump_->WriteRenderStreamMessage(
        AudioFrameView<const float>(src, num_channels, channel_size));
  }
  render_.render_audio->CopyFrom(src,
                                 formats_.api_format.reverse_input_stream());
  return ProcessRenderStreamLocked();
}

int AudioProcessingImpl::ProcessReverseStream(AudioFrame* frame) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessReverseStream_AudioFrame");
  rtc::CritScope cs(&crit_render_);
  if (frame == nullptr) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz &&
      frame->sample_rate_hz_ != kSampleRate48kHz) {
    return kBadSampleRateError;
  }

  if (frame->num_channels_ <= 0) {
    return kBadNumberChannelsError;
  }

  ProcessingConfig processing_config = formats_.api_format;
  processing_config.reverse_input_stream().set_sample_rate_hz(
      frame->sample_rate_hz_);
  processing_config.reverse_input_stream().set_num_channels(
      frame->num_channels_);
  processing_config.reverse_output_stream().set_sample_rate_hz(
      frame->sample_rate_hz_);
  processing_config.reverse_output_stream().set_num_channels(
      frame->num_channels_);

  RETURN_ON_ERR(MaybeInitializeRender(processing_config));
  if (frame->samples_per_channel_ !=
      formats_.api_format.reverse_input_stream().num_frames()) {
    return kBadDataLengthError;
  }

  if (aec_dump_) {
    aec_dump_->WriteRenderStreamMessage(*frame);
  }

  render_.render_audio->CopyFrom(frame);
  RETURN_ON_ERR(ProcessRenderStreamLocked());
  if (submodule_states_.RenderMultiBandProcessingActive() ||
      submodule_states_.RenderFullBandProcessingActive()) {
    render_.render_audio->CopyTo(frame);
  }
  return kNoError;
}

int AudioProcessingImpl::ProcessRenderStreamLocked() {
  AudioBuffer* render_buffer = render_.render_audio.get();  // For brevity.

  HandleRenderRuntimeSettings();

  if (submodules_.render_pre_processor) {
    submodules_.render_pre_processor->Process(render_buffer);
  }

  QueueNonbandedRenderAudio(render_buffer);

  if (submodule_states_.RenderMultiBandSubModulesActive() &&
      SampleRateSupportsMultiBand(
          formats_.render_processing_format.sample_rate_hz())) {
    render_buffer->SplitIntoFrequencyBands();
  }

  if (submodule_states_.RenderMultiBandSubModulesActive()) {
    QueueBandedRenderAudio(render_buffer);
  }

  // TODO(peah): Perform the queuing inside QueueRenderAudiuo().
  if (submodules_.echo_controller) {
    submodules_.echo_controller->AnalyzeRender(render_buffer);
  }

  if (submodule_states_.RenderMultiBandProcessingActive() &&
      SampleRateSupportsMultiBand(
          formats_.render_processing_format.sample_rate_hz())) {
    render_buffer->MergeFrequencyBands();
  }

  return kNoError;
}

int AudioProcessingImpl::set_stream_delay_ms(int delay) {
  rtc::CritScope cs(&crit_capture_);
  Error retval = kNoError;
  capture_.was_stream_delay_set = true;
  delay += capture_.delay_offset_ms;

  if (delay < 0) {
    delay = 0;
    retval = kBadStreamParameterWarning;
  }

  // TODO(ajm): the max is rather arbitrarily chosen; investigate.
  if (delay > 500) {
    delay = 500;
    retval = kBadStreamParameterWarning;
  }

  capture_nonlocked_.stream_delay_ms = delay;
  return retval;
}

bool AudioProcessingImpl::GetLinearAecOutput(
    rtc::ArrayView<std::array<float, 160>> linear_output) const {
  rtc::CritScope cs(&crit_capture_);
  AudioBuffer* linear_aec_buffer = capture_.linear_aec_output.get();

  RTC_DCHECK(linear_aec_buffer);
  if (linear_aec_buffer) {
    RTC_DCHECK_EQ(1, linear_aec_buffer->num_bands());
    RTC_DCHECK_EQ(linear_output.size(), linear_aec_buffer->num_channels());

    for (size_t ch = 0; ch < linear_aec_buffer->num_channels(); ++ch) {
      RTC_DCHECK_EQ(linear_output[ch].size(), linear_aec_buffer->num_frames());
      rtc::ArrayView<const float> channel_view =
          rtc::ArrayView<const float>(linear_aec_buffer->channels_const()[ch],
                                      linear_aec_buffer->num_frames());
      std::copy(channel_view.begin(), channel_view.end(),
                linear_output[ch].begin());
    }
    return true;
  }
  RTC_LOG(LS_ERROR) << "No linear AEC output available";
  RTC_NOTREACHED();
  return false;
}

int AudioProcessingImpl::stream_delay_ms() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_nonlocked_.stream_delay_ms;
}

bool AudioProcessingImpl::was_stream_delay_set() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_.was_stream_delay_set;
}

void AudioProcessingImpl::set_stream_key_pressed(bool key_pressed) {
  rtc::CritScope cs(&crit_capture_);
  capture_.key_pressed = key_pressed;
}

void AudioProcessingImpl::set_delay_offset_ms(int offset) {
  rtc::CritScope cs(&crit_capture_);
  capture_.delay_offset_ms = offset;
}

int AudioProcessingImpl::delay_offset_ms() const {
  rtc::CritScope cs(&crit_capture_);
  return capture_.delay_offset_ms;
}

void AudioProcessingImpl::set_stream_analog_level(int level) {
  rtc::CritScope cs_capture(&crit_capture_);

  if (submodules_.agc_manager) {
    submodules_.agc_manager->set_stream_analog_level(level);
    data_dumper_->DumpRaw("experimental_gain_control_set_stream_analog_level",
                          1, &level);
  } else {
    int error = submodules_.gain_control->set_stream_analog_level(level);
    RTC_DCHECK_EQ(kNoError, error);
  }
}

int AudioProcessingImpl::recommended_stream_analog_level() const {
  rtc::CritScope cs_capture(&crit_capture_);
  if (submodules_.agc_manager) {
    return submodules_.agc_manager->stream_analog_level();
  }
  return submodules_.gain_control->stream_analog_level();
}

void AudioProcessingImpl::AttachAecDump(std::unique_ptr<AecDump> aec_dump) {
  RTC_DCHECK(aec_dump);
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

  // The previously attached AecDump will be destroyed with the
  // 'aec_dump' parameter, which is after locks are released.
  aec_dump_.swap(aec_dump);
  WriteAecDumpConfigMessage(true);
  aec_dump_->WriteInitMessage(formats_.api_format, rtc::TimeUTCMillis());
}

void AudioProcessingImpl::DetachAecDump() {
  // The d-tor of a task-queue based AecDump blocks until all pending
  // tasks are done. This construction avoids blocking while holding
  // the render and capture locks.
  std::unique_ptr<AecDump> aec_dump = nullptr;
  {
    rtc::CritScope cs_render(&crit_render_);
    rtc::CritScope cs_capture(&crit_capture_);
    aec_dump = std::move(aec_dump_);
  }
}

void AudioProcessingImpl::AttachPlayoutAudioGenerator(
    std::unique_ptr<AudioGenerator> audio_generator) {
  // TODO(bugs.webrtc.org/8882) Stub.
  // Reset internal audio generator with audio_generator.
}

void AudioProcessingImpl::DetachPlayoutAudioGenerator() {
  // TODO(bugs.webrtc.org/8882) Stub.
  // Delete audio generator, if one is attached.
}

AudioProcessingStats AudioProcessingImpl::GetStatistics(
    bool has_remote_tracks) const {
  rtc::CritScope cs_capture(&crit_capture_);
  if (!has_remote_tracks) {
    return capture_.stats;
  }
  AudioProcessingStats stats = capture_.stats;
  EchoCancellationImpl::Metrics metrics;
  if (submodules_.echo_controller) {
    auto ec_metrics = submodules_.echo_controller->GetMetrics();
    stats.echo_return_loss = ec_metrics.echo_return_loss;
    stats.echo_return_loss_enhancement =
        ec_metrics.echo_return_loss_enhancement;
    stats.delay_ms = ec_metrics.delay_ms;
  }
  if (config_.residual_echo_detector.enabled) {
    RTC_DCHECK(submodules_.echo_detector);
    auto ed_metrics = submodules_.echo_detector->GetMetrics();
    stats.residual_echo_likelihood = ed_metrics.echo_likelihood;
    stats.residual_echo_likelihood_recent_max =
        ed_metrics.echo_likelihood_recent_max;
  }
  return stats;
}

void AudioProcessingImpl::MutateConfig(
    rtc::FunctionView<void(AudioProcessing::Config*)> mutator) {
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  mutator(&config_);
  ApplyConfig(config_);
}

AudioProcessing::Config AudioProcessingImpl::GetConfig() const {
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  return config_;
}

bool AudioProcessingImpl::UpdateActiveSubmoduleStates() {
  return submodule_states_.Update(
      config_.high_pass_filter.enabled, !!submodules_.echo_cancellation,
      !!submodules_.echo_control_mobile, config_.residual_echo_detector.enabled,
      !!submodules_.legacy_noise_suppressor || !!submodules_.noise_suppressor,
      submodules_.gain_control->is_enabled(), config_.gain_controller2.enabled,
      config_.pre_amplifier.enabled, capture_nonlocked_.echo_controller_enabled,
      config_.voice_detection.enabled, capture_.transient_suppressor_enabled);
}

void AudioProcessingImpl::InitializeTransient() {
  if (capture_.transient_suppressor_enabled) {
    if (!submodules_.transient_suppressor.get()) {
      submodules_.transient_suppressor.reset(new TransientSuppressor());
    }
    submodules_.transient_suppressor->Initialize(proc_fullband_sample_rate_hz(),
                                                 capture_nonlocked_.split_rate,
                                                 num_proc_channels());
  }
}

void AudioProcessingImpl::InitializeHighPassFilter() {
  bool high_pass_filter_needed_by_aec =
      config_.echo_canceller.enabled &&
      config_.echo_canceller.enforce_high_pass_filtering &&
      !config_.echo_canceller.mobile_mode;
  if (submodule_states_.HighPassFilteringRequired() ||
      high_pass_filter_needed_by_aec) {
    submodules_.high_pass_filter.reset(new HighPassFilter(num_proc_channels()));
  } else {
    submodules_.high_pass_filter.reset();
  }
}

void AudioProcessingImpl::InitializeVoiceDetector() {
  if (config_.voice_detection.enabled) {
    submodules_.voice_detector = std::make_unique<VoiceDetection>(
        proc_split_sample_rate_hz(), VoiceDetection::kVeryLowLikelihood);
  } else {
    submodules_.voice_detector.reset();
  }
}
void AudioProcessingImpl::InitializeEchoController() {
  bool use_echo_controller =
      echo_control_factory_ ||
      (config_.echo_canceller.enabled && !config_.echo_canceller.mobile_mode &&
       !config_.echo_canceller.use_legacy_aec);

  if (use_echo_controller) {
    // Create and activate the echo controller.
    if (echo_control_factory_) {
      submodules_.echo_controller = echo_control_factory_->Create(
          proc_sample_rate_hz(), num_reverse_channels(), num_proc_channels());
      RTC_DCHECK(submodules_.echo_controller);
    } else {
      EchoCanceller3Config config =
          use_setup_specific_default_aec3_config_
              ? EchoCanceller3::CreateDefaultConfig(num_reverse_channels(),
                                                    num_proc_channels())
              : EchoCanceller3Config();
      submodules_.echo_controller = std::make_unique<EchoCanceller3>(
          config, proc_sample_rate_hz(), num_reverse_channels(),
          num_proc_channels());
    }

    // Setup the storage for returning the linear AEC output.
    if (config_.echo_canceller.export_linear_aec_output) {
      constexpr int kLinearOutputRateHz = 16000;
      capture_.linear_aec_output = std::make_unique<AudioBuffer>(
          kLinearOutputRateHz, num_proc_channels(), kLinearOutputRateHz,
          num_proc_channels(), kLinearOutputRateHz, num_proc_channels());
    } else {
      capture_.linear_aec_output.reset();
    }

    capture_nonlocked_.echo_controller_enabled = true;

    submodules_.echo_cancellation.reset();
    aec_render_signal_queue_.reset();
    submodules_.echo_control_mobile.reset();
    aecm_render_signal_queue_.reset();
    return;
  }

  submodules_.echo_controller.reset();
  capture_nonlocked_.echo_controller_enabled = false;
  capture_.linear_aec_output.reset();

  if (!config_.echo_canceller.enabled) {
    submodules_.echo_cancellation.reset();
    aec_render_signal_queue_.reset();
    submodules_.echo_control_mobile.reset();
    aecm_render_signal_queue_.reset();
    return;
  }

  if (config_.echo_canceller.mobile_mode) {
    // Create and activate AECM.
    size_t max_element_size =
        std::max(static_cast<size_t>(1),
                 kMaxAllowedValuesOfSamplesPerBand *
                     EchoControlMobileImpl::NumCancellersRequired(
                         num_output_channels(), num_reverse_channels()));

    std::vector<int16_t> template_queue_element(max_element_size);

    aecm_render_signal_queue_.reset(
        new SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<int16_t>(max_element_size)));

    aecm_render_queue_buffer_.resize(max_element_size);
    aecm_capture_queue_buffer_.resize(max_element_size);

    submodules_.echo_control_mobile.reset(new EchoControlMobileImpl());

    submodules_.echo_control_mobile->Initialize(proc_split_sample_rate_hz(),
                                                num_reverse_channels(),
                                                num_output_channels());

    submodules_.echo_cancellation.reset();
    aec_render_signal_queue_.reset();
    return;
  }

  submodules_.echo_control_mobile.reset();
  aecm_render_signal_queue_.reset();

  // Create and activate AEC2.
  submodules_.echo_cancellation.reset(new EchoCancellationImpl());
  submodules_.echo_cancellation->SetExtraOptions(
      capture_nonlocked_.use_aec2_extended_filter,
      capture_nonlocked_.use_aec2_delay_agnostic,
      capture_nonlocked_.use_aec2_refined_adaptive_filter);

  size_t element_max_size =
      std::max(static_cast<size_t>(1),
               kMaxAllowedValuesOfSamplesPerBand *
                   EchoCancellationImpl::NumCancellersRequired(
                       num_output_channels(), num_reverse_channels()));

  std::vector<float> template_queue_element(element_max_size);

  aec_render_signal_queue_.reset(
      new SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>(
          kMaxNumFramesToBuffer, template_queue_element,
          RenderQueueItemVerifier<float>(element_max_size)));

  aec_render_queue_buffer_.resize(element_max_size);
  aec_capture_queue_buffer_.resize(element_max_size);

  submodules_.echo_cancellation->Initialize(
      proc_sample_rate_hz(), num_reverse_channels(), num_output_channels(),
      num_proc_channels());

  submodules_.echo_cancellation->set_suppression_level(
      config_.echo_canceller.legacy_moderate_suppression_level
          ? EchoCancellationImpl::SuppressionLevel::kModerateSuppression
          : EchoCancellationImpl::SuppressionLevel::kHighSuppression);
}

void AudioProcessingImpl::InitializeGainController2() {
  if (config_.gain_controller2.enabled) {
    submodules_.gain_controller2->Initialize(proc_fullband_sample_rate_hz());
  }
}

void AudioProcessingImpl::InitializeNoiseSuppressor() {
  submodules_.legacy_noise_suppressor.reset();
  submodules_.noise_suppressor.reset();

  if (config_.noise_suppression.enabled) {
    const bool use_legacy_ns =
        config_.noise_suppression.use_legacy_ns || enforced_usage_of_legacy_ns_;

    if (!use_legacy_ns) {
      auto map_level =
          [](AudioProcessing::Config::NoiseSuppression::Level level) {
            using NoiseSuppresionConfig =
                AudioProcessing::Config::NoiseSuppression;
            switch (level) {
              case NoiseSuppresionConfig::kLow:
                return NsConfig::SuppressionLevel::k6dB;
              case NoiseSuppresionConfig::kModerate:
                return NsConfig::SuppressionLevel::k12dB;
              case NoiseSuppresionConfig::kHigh:
                return NsConfig::SuppressionLevel::k18dB;
              case NoiseSuppresionConfig::kVeryHigh:
                return NsConfig::SuppressionLevel::k21dB;
              default:
                RTC_NOTREACHED();
            }
          };

      NsConfig cfg;
      cfg.target_level = map_level(config_.noise_suppression.level);
      submodules_.noise_suppressor = std::make_unique<NoiseSuppressor>(
          cfg, proc_sample_rate_hz(), num_proc_channels());
    } else {
      auto ns_level =
          NsConfigLevelToInterfaceLevel(config_.noise_suppression.level);
      submodules_.legacy_noise_suppressor = std::make_unique<NoiseSuppression>(
          num_proc_channels(), proc_sample_rate_hz(), ns_level);
    }
  }
}

void AudioProcessingImpl::InitializePreAmplifier() {
  if (config_.pre_amplifier.enabled) {
    submodules_.pre_amplifier.reset(
        new GainApplier(true, config_.pre_amplifier.fixed_gain_factor));
  } else {
    submodules_.pre_amplifier.reset();
  }
}

void AudioProcessingImpl::InitializeResidualEchoDetector() {
  RTC_DCHECK(submodules_.echo_detector);
  submodules_.echo_detector->Initialize(
      proc_fullband_sample_rate_hz(), 1,
      formats_.render_processing_format.sample_rate_hz(), 1);
}

void AudioProcessingImpl::InitializeAnalyzer() {
  if (submodules_.capture_analyzer) {
    submodules_.capture_analyzer->Initialize(proc_fullband_sample_rate_hz(),
                                             num_proc_channels());
  }
}

void AudioProcessingImpl::InitializePostProcessor() {
  if (submodules_.capture_post_processor) {
    submodules_.capture_post_processor->Initialize(
        proc_fullband_sample_rate_hz(), num_proc_channels());
  }
}

void AudioProcessingImpl::InitializePreProcessor() {
  if (submodules_.render_pre_processor) {
    submodules_.render_pre_processor->Initialize(
        formats_.render_processing_format.sample_rate_hz(),
        formats_.render_processing_format.num_channels());
  }
}

void AudioProcessingImpl::UpdateHistogramsOnCallEnd() {}

void AudioProcessingImpl::WriteAecDumpConfigMessage(bool forced) {
  if (!aec_dump_) {
    return;
  }

  std::string experiments_description = "";
  if (submodules_.echo_cancellation) {
    experiments_description +=
        submodules_.echo_cancellation->GetExperimentsDescription();
  }
  // TODO(peah): Add semicolon-separated concatenations of experiment
  // descriptions for other submodules.
  if (constants_.agc_clipped_level_min != kClippedLevelMin) {
    experiments_description += "AgcClippingLevelExperiment;";
  }
  if (capture_nonlocked_.echo_controller_enabled) {
    experiments_description += "EchoController;";
  }
  if (config_.gain_controller2.enabled) {
    experiments_description += "GainController2;";
  }

  InternalAPMConfig apm_config;

  apm_config.aec_enabled = config_.echo_canceller.enabled;
  apm_config.aec_delay_agnostic_enabled =
      submodules_.echo_cancellation &&
      submodules_.echo_cancellation->is_delay_agnostic_enabled();
  apm_config.aec_drift_compensation_enabled =
      submodules_.echo_cancellation &&
      submodules_.echo_cancellation->is_drift_compensation_enabled();
  apm_config.aec_extended_filter_enabled =
      submodules_.echo_cancellation &&
      submodules_.echo_cancellation->is_extended_filter_enabled();
  apm_config.aec_suppression_level =
      submodules_.echo_cancellation
          ? static_cast<int>(submodules_.echo_cancellation->suppression_level())
          : 0;

  apm_config.aecm_enabled = !!submodules_.echo_control_mobile;
  apm_config.aecm_comfort_noise_enabled =
      submodules_.echo_control_mobile &&
      submodules_.echo_control_mobile->is_comfort_noise_enabled();
  apm_config.aecm_routing_mode =
      submodules_.echo_control_mobile
          ? static_cast<int>(submodules_.echo_control_mobile->routing_mode())
          : 0;

  apm_config.agc_enabled = submodules_.gain_control->is_enabled();
  apm_config.agc_mode = static_cast<int>(submodules_.gain_control->mode());
  apm_config.agc_limiter_enabled =
      submodules_.gain_control->is_limiter_enabled();
  apm_config.noise_robust_agc_enabled = !!submodules_.agc_manager;

  apm_config.hpf_enabled = config_.high_pass_filter.enabled;

  apm_config.ns_enabled = config_.noise_suppression.enabled;
  apm_config.ns_level = static_cast<int>(config_.noise_suppression.level);

  apm_config.transient_suppression_enabled =
      capture_.transient_suppressor_enabled;
  apm_config.experiments_description = experiments_description;
  apm_config.pre_amplifier_enabled = config_.pre_amplifier.enabled;
  apm_config.pre_amplifier_fixed_gain_factor =
      config_.pre_amplifier.fixed_gain_factor;

  if (!forced && apm_config == apm_config_for_aec_dump_) {
    return;
  }
  aec_dump_->WriteConfig(apm_config);
  apm_config_for_aec_dump_ = apm_config;
}

void AudioProcessingImpl::RecordUnprocessedCaptureStream(
    const float* const* src) {
  RTC_DCHECK(aec_dump_);
  WriteAecDumpConfigMessage(false);

  const size_t channel_size = formats_.api_format.input_stream().num_frames();
  const size_t num_channels = formats_.api_format.input_stream().num_channels();
  aec_dump_->AddCaptureStreamInput(
      AudioFrameView<const float>(src, num_channels, channel_size));
  RecordAudioProcessingState();
}

void AudioProcessingImpl::RecordUnprocessedCaptureStream(
    const AudioFrame& capture_frame) {
  RTC_DCHECK(aec_dump_);
  WriteAecDumpConfigMessage(false);

  aec_dump_->AddCaptureStreamInput(capture_frame);
  RecordAudioProcessingState();
}

void AudioProcessingImpl::RecordProcessedCaptureStream(
    const float* const* processed_capture_stream) {
  RTC_DCHECK(aec_dump_);

  const size_t channel_size = formats_.api_format.output_stream().num_frames();
  const size_t num_channels =
      formats_.api_format.output_stream().num_channels();
  aec_dump_->AddCaptureStreamOutput(AudioFrameView<const float>(
      processed_capture_stream, num_channels, channel_size));
  aec_dump_->WriteCaptureStreamMessage();
}

void AudioProcessingImpl::RecordProcessedCaptureStream(
    const AudioFrame& processed_capture_frame) {
  RTC_DCHECK(aec_dump_);

  aec_dump_->AddCaptureStreamOutput(processed_capture_frame);
  aec_dump_->WriteCaptureStreamMessage();
}

void AudioProcessingImpl::RecordAudioProcessingState() {
  RTC_DCHECK(aec_dump_);
  AecDump::AudioProcessingState audio_proc_state;
  audio_proc_state.delay = capture_nonlocked_.stream_delay_ms;
  audio_proc_state.drift =
      submodules_.echo_cancellation
          ? submodules_.echo_cancellation->stream_drift_samples()
          : 0;
  audio_proc_state.level = recommended_stream_analog_level();
  audio_proc_state.keypress = capture_.key_pressed;
  aec_dump_->AddAudioProcessingState(audio_proc_state);
}

AudioProcessingImpl::ApmCaptureState::ApmCaptureState(
    bool transient_suppressor_enabled)
    : delay_offset_ms(0),
      was_stream_delay_set(false),
      output_will_be_muted(false),
      key_pressed(false),
      transient_suppressor_enabled(transient_suppressor_enabled),
      capture_processing_format(kSampleRate16kHz),
      split_rate(kSampleRate16kHz),
      echo_path_gain_change(false),
      prev_analog_mic_level(-1),
      prev_pre_amp_gain(-1.f),
      playout_volume(-1),
      prev_playout_volume(-1) {}

AudioProcessingImpl::ApmCaptureState::~ApmCaptureState() = default;

void AudioProcessingImpl::ApmCaptureState::KeyboardInfo::Extract(
    const float* const* data,
    const StreamConfig& stream_config) {
  if (stream_config.has_keyboard()) {
    keyboard_data = data[stream_config.num_channels()];
  } else {
    keyboard_data = NULL;
  }
  num_keyboard_frames = stream_config.num_frames();
}

AudioProcessingImpl::ApmRenderState::ApmRenderState() = default;

AudioProcessingImpl::ApmRenderState::~ApmRenderState() = default;

}  // namespace webrtc
