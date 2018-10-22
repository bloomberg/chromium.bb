// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/audio_processing_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<audio::mojom::AudioProcessingSettingsDataView,
                  media::AudioProcessingSettings>::
    Read(audio::mojom::AudioProcessingSettingsDataView data,
         media::AudioProcessingSettings* out_settings) {
  media::EchoCancellationType echo_cancellation;
  if (!data.ReadEchoCancellation(&echo_cancellation))
    return false;

  media::NoiseSuppressionType noise_suppression;
  if (!data.ReadNoiseSuppression(&noise_suppression))
    return false;

  media::AutomaticGainControlType automatic_gain_control;
  if (!data.ReadAutomaticGainControl(&automatic_gain_control))
    return false;

  *out_settings = media::AudioProcessingSettings{
      echo_cancellation,       noise_suppression,
      automatic_gain_control,  data.high_pass_filter(),
      data.typing_detection(), data.stereo_mirroring()};
  return true;
}

bool StructTraits<audio::mojom::AudioProcessingStatsDataView,
                  webrtc::AudioProcessorInterface::AudioProcessorStatistics>::
    Read(audio::mojom::AudioProcessingStatsDataView data,
         webrtc::AudioProcessorInterface::AudioProcessorStatistics* out_stats) {
  out_stats->typing_noise_detected = data.typing_noise_detected();
  auto& apm_stats = out_stats->apm_statistics;
  apm_stats = {};
  if (data.has_echo_return_loss())
    apm_stats.echo_return_loss = data.echo_return_loss();
  if (data.has_echo_return_loss_enhancement())
    apm_stats.echo_return_loss_enhancement =
        data.echo_return_loss_enhancement();
  if (data.has_divergent_filter_fraction())
    apm_stats.divergent_filter_fraction = data.divergent_filter_fraction();
  if (data.has_delay_median_ms())
    apm_stats.delay_median_ms = data.delay_median_ms();
  if (data.has_delay_standard_deviation_ms())
    apm_stats.delay_standard_deviation_ms = data.delay_standard_deviation_ms();
  if (data.has_residual_echo_likelihood())
    apm_stats.residual_echo_likelihood = data.residual_echo_likelihood();
  if (data.has_residual_echo_likelihood_recent_max())
    apm_stats.residual_echo_likelihood_recent_max =
        data.residual_echo_likelihood_recent_max();
  if (data.has_delay_ms())
    apm_stats.delay_ms = data.delay_ms();

  return true;
}

}  // namespace mojo
