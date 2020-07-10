/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
#define MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_

#include <list>
#include <memory>
#include <vector>

#include "api/function_view.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/agc/agc_manager_direct.h"
#include "modules/audio_processing/agc/gain_control.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/echo_cancellation_impl.h"
#include "modules/audio_processing/echo_control_mobile_impl.h"
#include "modules/audio_processing/gain_control_impl.h"
#include "modules/audio_processing/gain_controller2.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "modules/audio_processing/include/aec_dump.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "modules/audio_processing/legacy_noise_suppression.h"
#include "modules/audio_processing/level_estimator.h"
#include "modules/audio_processing/ns/noise_suppressor.h"
#include "modules/audio_processing/render_queue_item_verifier.h"
#include "modules/audio_processing/residual_echo_detector.h"
#include "modules/audio_processing/rms_level.h"
#include "modules/audio_processing/transient/transient_suppressor.h"
#include "modules/audio_processing/voice_detection.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/gtest_prod_util.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/swap_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class ApmDataDumper;
class AudioConverter;

class AudioProcessingImpl : public AudioProcessing {
 public:
  // Methods forcing APM to run in a single-threaded manner.
  // Acquires both the render and capture locks.
  explicit AudioProcessingImpl(const webrtc::Config& config);
  // AudioProcessingImpl takes ownership of capture post processor.
  AudioProcessingImpl(const webrtc::Config& config,
                      std::unique_ptr<CustomProcessing> capture_post_processor,
                      std::unique_ptr<CustomProcessing> render_pre_processor,
                      std::unique_ptr<EchoControlFactory> echo_control_factory,
                      rtc::scoped_refptr<EchoDetector> echo_detector,
                      std::unique_ptr<CustomAudioAnalyzer> capture_analyzer);
  ~AudioProcessingImpl() override;
  int Initialize() override;
  int Initialize(int capture_input_sample_rate_hz,
                 int capture_output_sample_rate_hz,
                 int render_sample_rate_hz,
                 ChannelLayout capture_input_layout,
                 ChannelLayout capture_output_layout,
                 ChannelLayout render_input_layout) override;
  int Initialize(const ProcessingConfig& processing_config) override;
  void ApplyConfig(const AudioProcessing::Config& config) override;
  void SetExtraOptions(const webrtc::Config& config) override;
  void UpdateHistogramsOnCallEnd() override;
  void AttachAecDump(std::unique_ptr<AecDump> aec_dump) override;
  void DetachAecDump() override;
  void AttachPlayoutAudioGenerator(
      std::unique_ptr<AudioGenerator> audio_generator) override;
  void DetachPlayoutAudioGenerator() override;

  void SetRuntimeSetting(RuntimeSetting setting) override;

  // Capture-side exclusive methods possibly running APM in a
  // multi-threaded manner. Acquire the capture lock.
  int ProcessStream(AudioFrame* frame) override;
  int ProcessStream(const float* const* src,
                    const StreamConfig& input_config,
                    const StreamConfig& output_config,
                    float* const* dest) override;
  bool GetLinearAecOutput(
      rtc::ArrayView<std::array<float, 160>> linear_output) const override;
  void set_output_will_be_muted(bool muted) override;
  int set_stream_delay_ms(int delay) override;
  void set_delay_offset_ms(int offset) override;
  int delay_offset_ms() const override;
  void set_stream_key_pressed(bool key_pressed) override;
  void set_stream_analog_level(int level) override;
  int recommended_stream_analog_level() const override;

  // Render-side exclusive methods possibly running APM in a
  // multi-threaded manner. Acquire the render lock.
  int ProcessReverseStream(AudioFrame* frame) override;
  int AnalyzeReverseStream(const float* const* data,
                           const StreamConfig& reverse_config) override;
  int ProcessReverseStream(const float* const* src,
                           const StreamConfig& input_config,
                           const StreamConfig& output_config,
                           float* const* dest) override;

  // Methods only accessed from APM submodules or
  // from AudioProcessing tests in a single-threaded manner.
  // Hence there is no need for locks in these.
  int proc_sample_rate_hz() const override;
  int proc_split_sample_rate_hz() const override;
  size_t num_input_channels() const override;
  size_t num_proc_channels() const override;
  size_t num_output_channels() const override;
  size_t num_reverse_channels() const override;
  int stream_delay_ms() const override;
  bool was_stream_delay_set() const override
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  AudioProcessingStats GetStatistics(bool has_remote_tracks) const override;

  // TODO(peah): Remove MutateConfig once the new API allows that.
  void MutateConfig(rtc::FunctionView<void(AudioProcessing::Config*)> mutator);
  AudioProcessing::Config GetConfig() const override;

 protected:
  // Overridden in a mock.
  virtual int InitializeLocked()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);

 private:
  // TODO(peah): These friend classes should be removed as soon as the new
  // parameter setting scheme allows.
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, DefaultBehavior);
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, ValidConfigBehavior);
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, InValidConfigBehavior);

  // Class providing thread-safe message pipe functionality for
  // |runtime_settings_|.
  class RuntimeSettingEnqueuer {
   public:
    explicit RuntimeSettingEnqueuer(
        SwapQueue<RuntimeSetting>* runtime_settings);
    ~RuntimeSettingEnqueuer();
    void Enqueue(RuntimeSetting setting);

   private:
    SwapQueue<RuntimeSetting>& runtime_settings_;
  };

  std::unique_ptr<ApmDataDumper> data_dumper_;
  static int instance_count_;
  const bool enforced_usage_of_legacy_ns_;
  const bool use_setup_specific_default_aec3_config_;

  SwapQueue<RuntimeSetting> capture_runtime_settings_;
  SwapQueue<RuntimeSetting> render_runtime_settings_;

  RuntimeSettingEnqueuer capture_runtime_settings_enqueuer_;
  RuntimeSettingEnqueuer render_runtime_settings_enqueuer_;

  // EchoControl factory.
  std::unique_ptr<EchoControlFactory> echo_control_factory_;

  class SubmoduleStates {
   public:
    SubmoduleStates(bool capture_post_processor_enabled,
                    bool render_pre_processor_enabled,
                    bool capture_analyzer_enabled);
    // Updates the submodule state and returns true if it has changed.
    bool Update(bool high_pass_filter_enabled,
                bool echo_canceller_enabled,
                bool mobile_echo_controller_enabled,
                bool residual_echo_detector_enabled,
                bool noise_suppressor_enabled,
                bool adaptive_gain_controller_enabled,
                bool gain_controller2_enabled,
                bool pre_amplifier_enabled,
                bool echo_controller_enabled,
                bool voice_detector_enabled,
                bool transient_suppressor_enabled);
    bool CaptureMultiBandSubModulesActive() const;
    bool CaptureMultiBandProcessingPresent() const;
    bool CaptureMultiBandProcessingActive(bool ec_processing_active) const;
    bool CaptureFullBandProcessingActive() const;
    bool CaptureAnalyzerActive() const;
    bool RenderMultiBandSubModulesActive() const;
    bool RenderFullBandProcessingActive() const;
    bool RenderMultiBandProcessingActive() const;
    bool HighPassFilteringRequired() const;

   private:
    const bool capture_post_processor_enabled_ = false;
    const bool render_pre_processor_enabled_ = false;
    const bool capture_analyzer_enabled_ = false;
    bool high_pass_filter_enabled_ = false;
    bool echo_canceller_enabled_ = false;
    bool mobile_echo_controller_enabled_ = false;
    bool residual_echo_detector_enabled_ = false;
    bool noise_suppressor_enabled_ = false;
    bool adaptive_gain_controller_enabled_ = false;
    bool gain_controller2_enabled_ = false;
    bool pre_amplifier_enabled_ = false;
    bool echo_controller_enabled_ = false;
    bool voice_detector_enabled_ = false;
    bool transient_suppressor_enabled_ = false;
    bool first_update_ = true;
  };

  // Method for modifying the formats struct that are called from both
  // the render and capture threads. The check for whether modifications
  // are needed is done while holding the render lock only, thereby avoiding
  // that the capture thread blocks the render thread.
  // The struct is modified in a single-threaded manner by holding both the
  // render and capture locks.
  int MaybeInitializeRender(const ProcessingConfig& processing_config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  // Method for updating the state keeping track of the active submodules.
  // Returns a bool indicating whether the state has changed.
  bool UpdateActiveSubmoduleStates()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Methods requiring APM running in a single-threaded manner.
  // Are called with both the render and capture locks already
  // acquired.
  void InitializeTransient()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  int InitializeLocked(const ProcessingConfig& config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void InitializeResidualEchoDetector()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void InitializeHighPassFilter() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializeVoiceDetector() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializeEchoController()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void InitializeGainController2() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializeNoiseSuppressor() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializePreAmplifier() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializePostProcessor() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializeAnalyzer() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializePreProcessor() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  // Sample rate used for the fullband processing.
  int proc_fullband_sample_rate_hz() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Empties and handles the respective RuntimeSetting queues.
  void HandleCaptureRuntimeSettings()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void HandleRenderRuntimeSettings() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_);
  void ApplyAgc1Config(const Config::GainController1& agc_config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  void EmptyQueuedRenderAudio();
  void AllocateRenderQueue()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void QueueBandedRenderAudio(AudioBuffer* audio)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_);
  void QueueNonbandedRenderAudio(AudioBuffer* audio)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  // Capture-side exclusive methods possibly running APM in a multi-threaded
  // manner that are called with the render lock already acquired.
  int ProcessCaptureStreamLocked() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Render-side exclusive methods possibly running APM in a multi-threaded
  // manner that are called with the render lock already acquired.
  // TODO(ekm): Remove once all clients updated to new interface.
  int AnalyzeReverseStreamLocked(const float* const* src,
                                 const StreamConfig& input_config,
                                 const StreamConfig& output_config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_);
  int ProcessRenderStreamLocked() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  // Collects configuration settings from public and private
  // submodules to be saved as an audioproc::Config message on the
  // AecDump if it is attached.  If not |forced|, only writes the current
  // config if it is different from the last saved one; if |forced|,
  // writes the config regardless of the last saved.
  void WriteAecDumpConfigMessage(bool forced)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Notifies attached AecDump of current configuration and capture data.
  void RecordUnprocessedCaptureStream(const float* const* capture_stream)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  void RecordUnprocessedCaptureStream(const AudioFrame& capture_frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Notifies attached AecDump of current configuration and
  // processed capture data and issues a capture stream recording
  // request.
  void RecordProcessedCaptureStream(
      const float* const* processed_capture_stream)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  void RecordProcessedCaptureStream(const AudioFrame& processed_capture_frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Notifies attached AecDump about current state (delay, drift, etc).
  void RecordAudioProcessingState() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // AecDump instance used for optionally logging APM config, input
  // and output to file in the AEC-dump format defined in debug.proto.
  std::unique_ptr<AecDump> aec_dump_;

  // Hold the last config written with AecDump for avoiding writing
  // the same config twice.
  InternalAPMConfig apm_config_for_aec_dump_ RTC_GUARDED_BY(crit_capture_);

  // Critical sections.
  rtc::CriticalSection crit_render_ RTC_ACQUIRED_BEFORE(crit_capture_);
  rtc::CriticalSection crit_capture_;

  // Struct containing the Config specifying the behavior of APM.
  AudioProcessing::Config config_;

  // Class containing information about what submodules are active.
  SubmoduleStates submodule_states_;

  // Struct containing the pointers to the submodules.
  struct Submodules {
    Submodules(std::unique_ptr<CustomProcessing> capture_post_processor,
               std::unique_ptr<CustomProcessing> render_pre_processor,
               rtc::scoped_refptr<EchoDetector> echo_detector,
               std::unique_ptr<CustomAudioAnalyzer> capture_analyzer)
        : echo_detector(std::move(echo_detector)),
          capture_post_processor(std::move(capture_post_processor)),
          render_pre_processor(std::move(render_pre_processor)),
          capture_analyzer(std::move(capture_analyzer)) {}
    // Accessed internally from capture or during initialization.
    std::unique_ptr<AgcManagerDirect> agc_manager;
    std::unique_ptr<GainControlImpl> gain_control;
    std::unique_ptr<GainController2> gain_controller2;
    std::unique_ptr<HighPassFilter> high_pass_filter;
    rtc::scoped_refptr<EchoDetector> echo_detector;
    std::unique_ptr<EchoCancellationImpl> echo_cancellation;
    std::unique_ptr<EchoControl> echo_controller;
    std::unique_ptr<EchoControlMobileImpl> echo_control_mobile;
    std::unique_ptr<NoiseSuppression> legacy_noise_suppressor;
    std::unique_ptr<NoiseSuppressor> noise_suppressor;
    std::unique_ptr<TransientSuppressor> transient_suppressor;
    std::unique_ptr<CustomProcessing> capture_post_processor;
    std::unique_ptr<CustomProcessing> render_pre_processor;
    std::unique_ptr<GainApplier> pre_amplifier;
    std::unique_ptr<CustomAudioAnalyzer> capture_analyzer;
    std::unique_ptr<LevelEstimator> output_level_estimator;
    std::unique_ptr<VoiceDetection> voice_detector;
  } submodules_;

  // State that is written to while holding both the render and capture locks
  // but can be read without any lock being held.
  // As this is only accessed internally of APM, and all internal methods in APM
  // either are holding the render or capture locks, this construct is safe as
  // it is not possible to read the variables while writing them.
  struct ApmFormatState {
    ApmFormatState()
        :  // Format of processing streams at input/output call sites.
          api_format({{{kSampleRate16kHz, 1, false},
                       {kSampleRate16kHz, 1, false},
                       {kSampleRate16kHz, 1, false},
                       {kSampleRate16kHz, 1, false}}}),
          render_processing_format(kSampleRate16kHz, 1) {}
    ProcessingConfig api_format;
    StreamConfig render_processing_format;
  } formats_;

  // APM constants.
  const struct ApmConstants {
    ApmConstants(int agc_startup_min_volume,
                 int agc_clipped_level_min,
                 bool use_experimental_agc,
                 bool use_experimental_agc_agc2_level_estimation,
                 bool use_experimental_agc_agc2_digital_adaptive,
                 bool multi_channel_render_support,
                 bool multi_channel_capture_support)
        : agc_startup_min_volume(agc_startup_min_volume),
          agc_clipped_level_min(agc_clipped_level_min),
          use_experimental_agc(use_experimental_agc),
          use_experimental_agc_agc2_level_estimation(
              use_experimental_agc_agc2_level_estimation),
          use_experimental_agc_agc2_digital_adaptive(
              use_experimental_agc_agc2_digital_adaptive),
          multi_channel_render_support(multi_channel_render_support),
          multi_channel_capture_support(multi_channel_capture_support) {}
    int agc_startup_min_volume;
    int agc_clipped_level_min;
    bool use_experimental_agc;
    bool use_experimental_agc_agc2_level_estimation;
    bool use_experimental_agc_agc2_digital_adaptive;
    bool multi_channel_render_support;
    bool multi_channel_capture_support;
  } constants_;

  struct ApmCaptureState {
    ApmCaptureState(bool transient_suppressor_enabled);
    ~ApmCaptureState();
    int delay_offset_ms;
    bool was_stream_delay_set;
    bool output_will_be_muted;
    bool key_pressed;
    bool transient_suppressor_enabled;
    std::unique_ptr<AudioBuffer> capture_audio;
    std::unique_ptr<AudioBuffer> capture_fullband_audio;
    std::unique_ptr<AudioBuffer> linear_aec_output;
    // Only the rate and samples fields of capture_processing_format_ are used
    // because the capture processing number of channels is mutable and is
    // tracked by the capture_audio_.
    StreamConfig capture_processing_format;
    int split_rate;
    bool echo_path_gain_change;
    int prev_analog_mic_level;
    float prev_pre_amp_gain;
    int playout_volume;
    int prev_playout_volume;
    AudioProcessingStats stats;
    struct KeyboardInfo {
      void Extract(const float* const* data, const StreamConfig& stream_config);
      size_t num_keyboard_frames = 0;
      const float* keyboard_data = nullptr;
    } keyboard_info;
  } capture_ RTC_GUARDED_BY(crit_capture_);

  struct ApmCaptureNonLockedState {
    ApmCaptureNonLockedState()
        : capture_processing_format(kSampleRate16kHz),
          split_rate(kSampleRate16kHz),
          stream_delay_ms(0) {}
    // Only the rate and samples fields of capture_processing_format_ are used
    // because the forward processing number of channels is mutable and is
    // tracked by the capture_audio_.
    StreamConfig capture_processing_format;
    int split_rate;
    int stream_delay_ms;
    bool echo_controller_enabled = false;
    bool use_aec2_extended_filter = false;
    bool use_aec2_delay_agnostic = false;
    bool use_aec2_refined_adaptive_filter = false;
  } capture_nonlocked_;

  struct ApmRenderState {
    ApmRenderState();
    ~ApmRenderState();
    std::unique_ptr<AudioConverter> render_converter;
    std::unique_ptr<AudioBuffer> render_audio;
  } render_ RTC_GUARDED_BY(crit_render_);

  std::vector<float> aec_render_queue_buffer_ RTC_GUARDED_BY(crit_render_);
  std::vector<float> aec_capture_queue_buffer_ RTC_GUARDED_BY(crit_capture_);

  std::vector<int16_t> aecm_render_queue_buffer_ RTC_GUARDED_BY(crit_render_);
  std::vector<int16_t> aecm_capture_queue_buffer_ RTC_GUARDED_BY(crit_capture_);

  size_t agc_render_queue_element_max_size_ RTC_GUARDED_BY(crit_render_)
      RTC_GUARDED_BY(crit_capture_) = 0;
  std::vector<int16_t> agc_render_queue_buffer_ RTC_GUARDED_BY(crit_render_);
  std::vector<int16_t> agc_capture_queue_buffer_ RTC_GUARDED_BY(crit_capture_);

  size_t red_render_queue_element_max_size_ RTC_GUARDED_BY(crit_render_)
      RTC_GUARDED_BY(crit_capture_) = 0;
  std::vector<float> red_render_queue_buffer_ RTC_GUARDED_BY(crit_render_);
  std::vector<float> red_capture_queue_buffer_ RTC_GUARDED_BY(crit_capture_);

  RmsLevel capture_input_rms_ RTC_GUARDED_BY(crit_capture_);
  RmsLevel capture_output_rms_ RTC_GUARDED_BY(crit_capture_);
  int capture_rms_interval_counter_ RTC_GUARDED_BY(crit_capture_) = 0;

  // Lock protection not needed.
  std::unique_ptr<SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>>
      aec_render_signal_queue_;
  std::unique_ptr<
      SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>>
      aecm_render_signal_queue_;
  std::unique_ptr<
      SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>>
      agc_render_signal_queue_;
  std::unique_ptr<SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>>
      red_render_signal_queue_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
