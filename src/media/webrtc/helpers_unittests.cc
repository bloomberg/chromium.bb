// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/helpers.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
using Agc2Config = webrtc::AudioProcessing::Config::GainController2;
using ClippingPredictor = webrtc::AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;

constexpr WebRtcHybridAgcParams kHybridAgcParams{
    .dry_run = false,
    .vad_reset_period_ms = 1500,
    .adjacent_speech_frames_threshold = 12,
    .max_gain_change_db_per_second = 3.0f,
    .max_output_noise_level_dbfs = -50.0f,
    .sse2_allowed = true,
    .avx2_allowed = true,
    .neon_allowed = true};

constexpr AudioProcessingSettings kAudioProcessingNoAgc{
    .automatic_gain_control = false,
    .experimental_automatic_gain_control = false};

constexpr AudioProcessingSettings kAudioProcessingNoExperimentalAgc{
    .automatic_gain_control = true,
    .experimental_automatic_gain_control = false};

constexpr AudioProcessingSettings kAudioProcessingExperimentalAgc{
    .automatic_gain_control = true,
    .experimental_automatic_gain_control = true};

constexpr WebRtcAnalogAgcClippingControlParams kClippingControlParams{
    .mode = 2,  // ClippingPredictor::Mode::kFixedStepClippingPeakPrediction
    .window_length = 111,
    .reference_window_length = 222,
    .reference_window_delay = 333,
    .clipping_threshold = 4.44f,
    .crest_factor_margin = 5.55f,
    .clipped_level_step = 666,
    .clipped_ratio_threshold = 0.777f,
    .clipped_wait_frames = 300,
    .use_predicted_step = true};
}  // namespace

TEST(ConfigAutomaticGainControlTest, NoEffectIfGainControlNotEnabled) {
  constexpr webrtc::AudioProcessing::Config kDefaultConfig;
  webrtc::AudioProcessing::Config apm_config;

  ConfigAutomaticGainControl(kAudioProcessingNoAgc, kHybridAgcParams,
                             kClippingControlParams, apm_config);
  EXPECT_EQ(apm_config.gain_controller1, kDefaultConfig.gain_controller1);
  EXPECT_EQ(apm_config.gain_controller2, kDefaultConfig.gain_controller2);

  ConfigAutomaticGainControl(kAudioProcessingNoAgc,
                             /*hybrid_agc_params=*/absl::nullopt,
                             /*clipping_control_params=*/absl::nullopt,
                             apm_config);
  EXPECT_EQ(apm_config.gain_controller1, kDefaultConfig.gain_controller1);
  EXPECT_EQ(apm_config.gain_controller2, kDefaultConfig.gain_controller2);
}

TEST(ConfigAutomaticGainControlTest, EnableDefaultAGC1) {
  webrtc::AudioProcessing::Config apm_config;

  ConfigAutomaticGainControl(kAudioProcessingNoExperimentalAgc,
                             /*hybrid_agc_params=*/absl::nullopt,
                             /*clipping_control_params=*/absl::nullopt,
                             apm_config);

  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_EQ(
      apm_config.gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)

  EXPECT_FALSE(apm_config.gain_controller1.analog_gain_controller
                   .clipping_predictor.enabled);
}

TEST(ConfigAutomaticGainControlTest, EnableHybridAGC) {
  webrtc::AudioProcessing::Config apm_config;

  ConfigAutomaticGainControl(kAudioProcessingExperimentalAgc, kHybridAgcParams,
                             /*clipping_control_params=*/absl::nullopt,
                             apm_config);
  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_EQ(
      apm_config.gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)
  EXPECT_TRUE(apm_config.gain_controller2.enabled);
  EXPECT_TRUE(apm_config.gain_controller2.adaptive_digital.enabled);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.dry_run,
            kHybridAgcParams.dry_run);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.vad_reset_period_ms,
            kHybridAgcParams.vad_reset_period_ms);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital
                .adjacent_speech_frames_threshold,
            kHybridAgcParams.adjacent_speech_frames_threshold);
  EXPECT_FLOAT_EQ(apm_config.gain_controller2.adaptive_digital
                      .max_gain_change_db_per_second,
                  kHybridAgcParams.max_gain_change_db_per_second);
  EXPECT_FLOAT_EQ(
      apm_config.gain_controller2.adaptive_digital.max_output_noise_level_dbfs,
      kHybridAgcParams.max_output_noise_level_dbfs);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.sse2_allowed,
            kHybridAgcParams.sse2_allowed);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.avx2_allowed,
            kHybridAgcParams.avx2_allowed);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.neon_allowed,
            kHybridAgcParams.neon_allowed);
}

TEST(ConfigAutomaticGainControlTest, EnableClippingControl) {
  webrtc::AudioProcessing::Config apm_config;
  ConfigAutomaticGainControl(kAudioProcessingExperimentalAgc,
                             /*hybrid_agc_params=*/absl::nullopt,
                             kClippingControlParams, apm_config);
  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_TRUE(apm_config.gain_controller1.analog_gain_controller.enabled);

  const auto& analog_gain_controller =
      apm_config.gain_controller1.analog_gain_controller;
  EXPECT_EQ(analog_gain_controller.clipped_level_step,
            kClippingControlParams.clipped_level_step);
  EXPECT_FLOAT_EQ(analog_gain_controller.clipped_ratio_threshold,
                  kClippingControlParams.clipped_ratio_threshold);
  EXPECT_EQ(analog_gain_controller.clipped_wait_frames,
            kClippingControlParams.clipped_wait_frames);

  const auto& clipping_predictor = analog_gain_controller.clipping_predictor;
  EXPECT_TRUE(clipping_predictor.enabled);
  EXPECT_EQ(clipping_predictor.mode,
            ClippingPredictor::Mode::kFixedStepClippingPeakPrediction);
  EXPECT_EQ(clipping_predictor.window_length,
            kClippingControlParams.window_length);
  EXPECT_EQ(clipping_predictor.reference_window_length,
            kClippingControlParams.reference_window_length);
  EXPECT_EQ(clipping_predictor.reference_window_delay,
            kClippingControlParams.reference_window_delay);
  EXPECT_FLOAT_EQ(clipping_predictor.clipping_threshold,
                  kClippingControlParams.clipping_threshold);
  EXPECT_FLOAT_EQ(clipping_predictor.crest_factor_margin,
                  kClippingControlParams.crest_factor_margin);
  EXPECT_EQ(clipping_predictor.use_predicted_step,
            kClippingControlParams.use_predicted_step);
}

// Verify that the default settings in AudioProcessingSettings are applied
// correctly by CreateWebRtcAudioProcessingModule().
TEST(CreateWebRtcAudioProcessingModuleTest,
     CheckDefaultAudioProcessingSettings) {
  const AudioProcessingSettings default_settings;
  rtc::scoped_refptr<webrtc::AudioProcessing> apm =
      CreateWebRtcAudioProcessingModule(
          default_settings,
          /*agc_startup_min_volume=*/absl::nullopt);
  ASSERT_TRUE(!!apm);

  webrtc::AudioProcessing::Config config = apm->GetConfig();

  EXPECT_TRUE(config.pipeline.multi_channel_render);
  EXPECT_TRUE(config.pipeline.multi_channel_capture);
  EXPECT_EQ(config.pipeline.maximum_internal_processing_rate, 48000);
  EXPECT_TRUE(config.high_pass_filter.enabled);
  EXPECT_FALSE(config.pre_amplifier.enabled);
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller1.analog_gain_controller.enabled);
  EXPECT_FALSE(config.gain_controller2.enabled);
  EXPECT_TRUE(config.noise_suppression.enabled);
  EXPECT_EQ(config.noise_suppression.level,
            webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);
  EXPECT_FALSE(config.voice_detection.enabled);
  EXPECT_FALSE(config.residual_echo_detector.enabled);

#if defined(OS_ANDROID)
  // Android uses echo cancellation optimized for mobiles, and does not
  // support keytap suppression.
  EXPECT_TRUE(config.echo_canceller.mobile_mode);
  EXPECT_FALSE(config.transient_suppression.enabled);
#else
  EXPECT_FALSE(config.echo_canceller.mobile_mode);
  EXPECT_TRUE(config.transient_suppression.enabled);
#endif
}

TEST(CreateWebRtcAudioProcessingModuleTest, VerifyNoiseSuppressionSettings) {
  for (bool ns_enabled : {true, false}) {
    const AudioProcessingSettings settings{.noise_suppression = ns_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

    EXPECT_EQ(config.noise_suppression.enabled, ns_enabled);
    EXPECT_EQ(config.noise_suppression.level,
              webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, VerifyEchoCancellerSettings) {
  for (bool ec_enabled : {true, false}) {
    const AudioProcessingSettings settings{.echo_cancellation = ec_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

    EXPECT_EQ(config.echo_canceller.enabled, ec_enabled);
#if defined(OS_ANDROID)
    EXPECT_TRUE(config.echo_canceller.mobile_mode);
#else
    EXPECT_FALSE(config.echo_canceller.mobile_mode);
#endif
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, ToggleHighPassFilter) {
  for (bool high_pass_filter_enabled : {true, false}) {
    const AudioProcessingSettings settings{.high_pass_filter =
                                               high_pass_filter_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

    EXPECT_EQ(config.high_pass_filter.enabled, high_pass_filter_enabled);
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, ToggleTransientSuppression) {
  for (bool transient_suppression_enabled : {true, false}) {
    const AudioProcessingSettings settings{.transient_noise_suppression =
                                               transient_suppression_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

#if !defined(OS_IOS) && !defined(OS_ANDROID)
    EXPECT_EQ(config.transient_suppression.enabled,
              transient_suppression_enabled);
#else
    // Transient suppression is not supported (nor useful) on mobile platforms.
    EXPECT_FALSE(config.transient_suppression.enabled);
#endif
  }
}
}  // namespace media
