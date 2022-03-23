// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/audio_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/webrtc/constants.h"
#include "media/webrtc/helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace media {

namespace {
constexpr int kBuffersPerSecond = 100;  // 10 ms per buffer.
}  // namespace

// Wraps AudioBus to provide access to the array of channel pointers, since this
// is the type webrtc::AudioProcessing deals in. The array is refreshed on every
// channel_ptrs() call, and will be valid until the underlying AudioBus pointers
// are changed, e.g. through calls to SetChannelData() or SwapChannels().
// After construction, all methods are called on a single sequence.
class AudioProcessorCaptureBus {
 public:
  AudioProcessorCaptureBus(int channels, int frames)
      : bus_(media::AudioBus::Create(channels, frames)),
        channel_ptrs_(new float*[channels]) {
    bus_->Zero();
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  media::AudioBus* bus() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return bus_.get();
  }

  float* const* channel_ptrs() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (int i = 0; i < bus_->channels(); ++i) {
      channel_ptrs_[i] = bus_->channel(i);
    }
    return channel_ptrs_.get();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<media::AudioBus> bus_;
  std::unique_ptr<float*[]> channel_ptrs_;
};

// Wraps AudioFifo to provide a cleaner interface to AudioProcessor.
// It avoids the FIFO when the source and destination frames match. If
// |source_channels| is larger than |destination_channels|, only the first
// |destination_channels| are kept from the source.
// After construction, all methods are called sequentially.
class AudioProcessorCaptureFifo {
 public:
  AudioProcessorCaptureFifo(int source_channels,
                            int destination_channels,
                            int source_frames,
                            int destination_frames,
                            int sample_rate)
      :
#if DCHECK_IS_ON()
        source_channels_(source_channels),
        source_frames_(source_frames),
#endif
        sample_rate_(sample_rate),
        destination_(
            std::make_unique<AudioProcessorCaptureBus>(destination_channels,
                                                       destination_frames)),
        data_available_(false) {
    DCHECK_GE(source_channels, destination_channels);

    if (source_channels > destination_channels) {
      audio_source_intermediate_ =
          media::AudioBus::CreateWrapper(destination_channels);
    }

    if (source_frames != destination_frames) {
      // Since we require every Push to be followed by as many Consumes as
      // possible, twice the larger of the two is a (probably) loose upper bound
      // on the FIFO size.
      const int fifo_frames = 2 * std::max(source_frames, destination_frames);
      fifo_ =
          std::make_unique<media::AudioFifo>(destination_channels, fifo_frames);
    }

    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  void Push(const media::AudioBus& source, base::TimeDelta audio_delay) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
    DCHECK_EQ(source.channels(), source_channels_);
    DCHECK_EQ(source.frames(), source_frames_);
#endif
    const media::AudioBus* source_to_push = &source;

    if (audio_source_intermediate_) {
      for (int i = 0; i < destination_->bus()->channels(); ++i) {
        audio_source_intermediate_->SetChannelData(
            i, const_cast<float*>(source.channel(i)));
      }
      audio_source_intermediate_->set_frames(source.frames());
      source_to_push = audio_source_intermediate_.get();
    }

    if (fifo_) {
      CHECK_LT(fifo_->frames(), destination_->bus()->frames());
      next_audio_delay_ =
          audio_delay + fifo_->frames() * base::Seconds(1) / sample_rate_;
      fifo_->Push(source_to_push);
    } else {
      CHECK(!data_available_);
      source_to_push->CopyTo(destination_->bus());
      next_audio_delay_ = audio_delay;
      data_available_ = true;
    }
  }

  // Returns true if there are destination_frames() of data available to be
  // consumed, and otherwise false.
  bool Consume(AudioProcessorCaptureBus** destination,
               base::TimeDelta* audio_delay) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (fifo_) {
      if (fifo_->frames() < destination_->bus()->frames())
        return false;

      fifo_->Consume(destination_->bus(), 0, destination_->bus()->frames());
      *audio_delay = next_audio_delay_;
      next_audio_delay_ -=
          destination_->bus()->frames() * base::Seconds(1) / sample_rate_;
    } else {
      if (!data_available_)
        return false;
      *audio_delay = next_audio_delay_;
      // The data was already copied to |destination_| in this case.
      data_available_ = false;
    }

    *destination = destination_.get();
    return true;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
#if DCHECK_IS_ON()
  const int source_channels_;
  const int source_frames_;
#endif
  const int sample_rate_;
  std::unique_ptr<media::AudioBus> audio_source_intermediate_;
  std::unique_ptr<AudioProcessorCaptureBus> destination_;
  std::unique_ptr<media::AudioFifo> fifo_;

  // When using |fifo_|, this is the audio delay of the first sample to be
  // consumed next from the FIFO.  When not using |fifo_|, this is the audio
  // delay of the first sample in |destination_|.
  base::TimeDelta next_audio_delay_;

  // True when |destination_| contains the data to be returned by the next call
  // to Consume().  Only used when the FIFO is disabled.
  bool data_available_;
};

// static
std::unique_ptr<AudioProcessor> AudioProcessor::Create(
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback,
    const AudioProcessingSettings& settings,
    const media::AudioParameters& input_format,
    const media::AudioParameters& output_format) {
  log_callback.Run(base::StringPrintf(
      "AudioProcessor::Create({multi_channel_capture_processing=%s})",
      settings.multi_channel_capture_processing ? "true" : "false"));

  rtc::scoped_refptr<webrtc::AudioProcessing> webrtc_audio_processing =
      media::CreateWebRtcAudioProcessingModule(settings);

  return std::make_unique<AudioProcessor>(
      std::move(deliver_processed_audio_callback), std::move(log_callback),
      input_format, output_format, std::move(webrtc_audio_processing),
      settings.stereo_mirroring);
}

AudioProcessor::AudioProcessor(
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback,
    const media::AudioParameters& input_format,
    const media::AudioParameters& output_format,
    rtc::scoped_refptr<webrtc::AudioProcessing> webrtc_audio_processing,
    bool stereo_mirroring)
    : webrtc_audio_processing_(webrtc_audio_processing),
      stereo_mirroring_(stereo_mirroring),
      log_callback_(std::move(log_callback)),
      input_format_(input_format),
      output_format_(output_format),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      audio_delay_stats_reporter_(kBuffersPerSecond),
      playout_fifo_(
          // Unretained is safe, since the callback is always called
          // synchronously within the AudioProcessor.
          base::BindRepeating(&AudioProcessor::AnalyzePlayoutData,
                              base::Unretained(this))) {
  DCHECK(deliver_processed_audio_callback_);
  DCHECK(log_callback_);

  CHECK(input_format_.IsValid());
  CHECK(output_format_.IsValid());
  if (webrtc_audio_processing_) {
    DCHECK_EQ(output_format_.sample_rate() / 100,
              output_format_.frames_per_buffer());
  }
  SendLogMessage(base::StringPrintf(
      "%s({input_format_=[%s], output_format_=[%s]})", __func__,
      input_format_.AsHumanReadableString().c_str(),
      output_format_.AsHumanReadableString().c_str()));

  // If audio processing is needed, rebuffer to 10 ms. If not, rebuffer to the
  // requested output format.
  const int fifo_output_frames_per_buffer =
      webrtc_audio_processing_ ? input_format_.sample_rate() / 100
                               : output_format_.frames_per_buffer();
  SendLogMessage(base::StringPrintf(
      "%s => (capture FIFO: fifo_output_frames_per_buffer=%d)", __func__,
      fifo_output_frames_per_buffer));
  capture_fifo_ = std::make_unique<AudioProcessorCaptureFifo>(
      input_format.channels(), input_format_.channels(),
      input_format.frames_per_buffer(), fifo_output_frames_per_buffer,
      input_format.sample_rate());

  if (webrtc_audio_processing_) {
    output_bus_ = std::make_unique<AudioProcessorCaptureBus>(
        output_format_.channels(), output_format.frames_per_buffer());
  }
}

AudioProcessor::~AudioProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  OnStopDump();
}

void AudioProcessor::ProcessCapturedAudio(const media::AudioBus& audio_source,
                                          base::TimeTicks audio_capture_time,
                                          int num_preferred_channels,
                                          double volume,
                                          bool key_pressed) {
  DCHECK(deliver_processed_audio_callback_);
  // Sanity-check the input audio format in debug builds.
  DCHECK(input_format_.IsValid());
  DCHECK_EQ(audio_source.channels(), input_format_.channels());
  DCHECK_EQ(audio_source.frames(), input_format_.frames_per_buffer());

  base::TimeDelta capture_delay = base::TimeTicks::Now() - audio_capture_time;
  TRACE_EVENT1("audio", "AudioProcessor::ProcessCapturedAudio", "delay (ms)",
               capture_delay.InMillisecondsF());

  capture_fifo_->Push(audio_source, capture_delay);

  // Process and consume the data in the FIFO until there is not enough
  // data to process.
  AudioProcessorCaptureBus* process_bus;
  while (capture_fifo_->Consume(&process_bus, &capture_delay)) {
    // Use the process bus directly if audio processing is disabled.
    AudioProcessorCaptureBus* output_bus = process_bus;
    absl::optional<double> new_volume;
    if (webrtc_audio_processing_) {
      output_bus = output_bus_.get();
      new_volume =
          ProcessData(process_bus->channel_ptrs(), process_bus->bus()->frames(),
                      capture_delay, volume, key_pressed,
                      num_preferred_channels, output_bus->channel_ptrs());
    }

    // Swap channels before interleaving the data.
    if (stereo_mirroring_ &&
        output_format_.channel_layout() == media::CHANNEL_LAYOUT_STEREO) {
      // Swap the first and second channels.
      output_bus->bus()->SwapChannels(0, 1);
    }

    deliver_processed_audio_callback_.Run(*output_bus->bus(),
                                          audio_capture_time, new_volume);
  }
}

const media::AudioParameters& AudioProcessor::OutputFormat() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return output_format_;
}

void AudioProcessor::SetOutputWillBeMuted(bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage(
      base::StringPrintf("%s({muted=%s})", __func__, muted ? "true" : "false"));
  if (webrtc_audio_processing_) {
    webrtc_audio_processing_->set_output_will_be_muted(muted);
  }
}

void AudioProcessor::OnStartDump(base::File dump_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(dump_file.IsValid());

  if (webrtc_audio_processing_) {
    if (!worker_queue_) {
      worker_queue_ = std::make_unique<rtc::TaskQueue>(
          CreateWebRtcTaskQueue(rtc::TaskQueue::Priority::LOW));
    }
    // Here tasks will be posted on the |worker_queue_|. It must be
    // kept alive until media::StopEchoCancellationDump is called or the
    // webrtc::AudioProcessing instance is destroyed.
    media::StartEchoCancellationDump(webrtc_audio_processing_.get(),
                                     std::move(dump_file), worker_queue_.get());
  } else {
    // Post the file close to avoid blocking the control sequence.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::LOWEST, base::MayBlock()},
        base::BindOnce([](base::File) {}, std::move(dump_file)));
  }
}

void AudioProcessor::OnStopDump() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!worker_queue_)
    return;
  if (webrtc_audio_processing_)
    media::StopEchoCancellationDump(webrtc_audio_processing_.get());
  worker_queue_.reset(nullptr);
}

void AudioProcessor::OnPlayoutData(const AudioBus& audio_bus,
                                   int sample_rate,
                                   base::TimeDelta audio_delay) {
  TRACE_EVENT1("audio", "AudioProcessor::OnPlayoutData", "delay (ms)",
               audio_delay.InMillisecondsF());

  if (!webrtc_audio_processing_) {
    return;
  }

  unbuffered_playout_delay_ = audio_delay;

  if (!playout_sample_rate_hz_ || sample_rate != *playout_sample_rate_hz_) {
    // We reset the buffer on sample rate changes because the current buffer
    // content is rendered obsolete (the audio processing module will reset
    // internally) and the FIFO does not resample previous content to the new
    // rate.
    // Channel count changes are already handled within the AudioPushFifo.
    playout_sample_rate_hz_ = sample_rate;
    const int num_frames_per_10_ms = sample_rate / 100;
    playout_fifo_.Reset(num_frames_per_10_ms);
  }

  playout_fifo_.Push(audio_bus);
}

void AudioProcessor::AnalyzePlayoutData(const AudioBus& audio_bus,
                                        int frame_delay) {
  DCHECK(webrtc_audio_processing_);
  DCHECK(playout_sample_rate_hz_.has_value());

  const base::TimeDelta playout_delay =
      unbuffered_playout_delay_ +
      AudioTimestampHelper::FramesToTime(frame_delay, *playout_sample_rate_hz_);
  playout_delay_ = playout_delay;
  TRACE_EVENT1("audio", "AudioProcessor::AnalyzePlayoutData", "delay (ms)",
               playout_delay.InMillisecondsF());

  webrtc::StreamConfig input_stream_config(*playout_sample_rate_hz_,
                                           audio_bus.channels());
  // If the input audio appears to contain upmixed mono audio, then APM is only
  // given the left channel. This reduces computational complexity and improves
  // convergence of audio processing algorithms.
  assume_upmixed_mono_playout_ = assume_upmixed_mono_playout_ &&
                                 LeftAndRightChannelsAreSymmetric(audio_bus);
  if (assume_upmixed_mono_playout_) {
    input_stream_config.set_num_channels(1);
  }

  std::array<const float*, media::limits::kMaxChannels> input_ptrs;
  for (int i = 0; i < static_cast<int>(input_stream_config.num_channels()); ++i)
    input_ptrs[i] = audio_bus.channel(i);

  const int apm_error = webrtc_audio_processing_->AnalyzeReverseStream(
      input_ptrs.data(), input_stream_config);
  if (apm_error != webrtc::AudioProcessing::kNoError &&
      apm_playout_error_code_log_count_ < 10) {
    LOG(ERROR) << "MSAP::OnPlayoutData: AnalyzeReverseStream error="
               << apm_error;
    ++apm_playout_error_code_log_count_;
  }
}

webrtc::AudioProcessingStats AudioProcessor::GetStats() {
  if (!webrtc_audio_processing_)
    return {};
  return webrtc_audio_processing_->GetStatistics();
}

absl::optional<double> AudioProcessor::ProcessData(
    const float* const* process_ptrs,
    int process_frames,
    base::TimeDelta capture_delay,
    double volume,
    bool key_pressed,
    int num_preferred_channels,
    float* const* output_ptrs) {
  DCHECK(webrtc_audio_processing_);

  const base::TimeDelta playout_delay = playout_delay_;

  TRACE_EVENT2("audio", "AudioProcessor::ProcessData", "capture_delay (ms)",
               capture_delay.InMillisecondsF(), "playout_delay (ms)",
               playout_delay.InMillisecondsF());

  const int64_t total_delay_ms =
      (capture_delay + playout_delay).InMilliseconds();

  if (total_delay_ms > 300 && large_delay_log_count_ < 10) {
    LOG(WARNING) << "Large audio delay, capture delay: "
                 << capture_delay.InMillisecondsF()
                 << "ms; playout delay: " << playout_delay.InMillisecondsF()
                 << "ms";
    ++large_delay_log_count_;
  }

  audio_delay_stats_reporter_.ReportDelay(capture_delay, playout_delay);

  webrtc::AudioProcessing* ap = webrtc_audio_processing_.get();
  DCHECK_LE(total_delay_ms, std::numeric_limits<int>::max());
  ap->set_stream_delay_ms(base::saturated_cast<int>(total_delay_ms));

  // Keep track of the maximum number of preferred channels. The number of
  // output channels of APM can increase if preferred by the sinks, but
  // never decrease.
  max_num_preferred_output_channels_ =
      std::max(max_num_preferred_output_channels_, num_preferred_channels);

  // Upscale the volume to the range expected by the WebRTC automatic gain
  // controller.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  DCHECK_LE(volume, 1.0);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_OPENBSD)
  // We have a special situation on Linux where the microphone volume can be
  // "higher than maximum". The input volume slider in the sound preference
  // allows the user to set a scaling that is higher than 100%. It means that
  // even if the reported maximum levels is N, the actual microphone level can
  // go up to 1.5x*N and that corresponds to a normalized |volume| of 1.5x.
  DCHECK_LE(volume, 1.6);
#endif
  // Map incoming volume range of [0.0, 1.0] to [0, 255] used by AGC.
  // The volume can be higher than 255 on Linux, and it will be cropped to
  // 255 since AGC does not allow values out of range.
  const int max_analog_gain_level = media::MaxWebRtcAnalogGainLevel();
  int current_analog_gain_level =
      static_cast<int>((volume * max_analog_gain_level) + 0.5);
  current_analog_gain_level =
      std::min(current_analog_gain_level, max_analog_gain_level);
  DCHECK_LE(current_analog_gain_level, max_analog_gain_level);

  ap->set_stream_analog_level(current_analog_gain_level);
  ap->set_stream_key_pressed(key_pressed);

  // Depending on how many channels the sinks prefer, the number of APM output
  // channels is allowed to vary between 1 and the number of channels of the
  // output format. The output format in turn depends on the input format.
  // Example: With a stereo mic the output format will have 2 channels, and APM
  // will produce 1 or 2 output channels depending on the sinks.
  int num_apm_output_channels =
      std::min(max_num_preferred_output_channels_, output_format_.channels());

  // Limit number of apm output channels to 2 to avoid potential problems with
  // discrete channel mapping.
  num_apm_output_channels = std::min(num_apm_output_channels, 2);

  CHECK_GE(num_apm_output_channels, 1);
  const webrtc::StreamConfig apm_output_config = webrtc::StreamConfig(
      output_format_.sample_rate(), num_apm_output_channels);

  int err = ap->ProcessStream(process_ptrs, CreateStreamConfig(input_format_),
                              apm_output_config, output_ptrs);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  // Upmix if the number of channels processed by APM is less than the number
  // specified in the output format. Channels above stereo will be set to zero.
  if (num_apm_output_channels < output_format_.channels()) {
    if (num_apm_output_channels == 1) {
      // The right channel is a copy of the left channel. Remaining channels
      // have already been set to zero at initialization.
      memcpy(&output_ptrs[1][0], &output_ptrs[0][0],
             output_format_.frames_per_buffer() * sizeof(output_ptrs[0][0]));
    }
  }

  // Return a new mic volume, if the volume has been changed.
  const int recommended_analog_gain_level =
      ap->recommended_stream_analog_level();
  if (recommended_analog_gain_level == current_analog_gain_level) {
    return absl::nullopt;
  } else {
    return static_cast<double>(recommended_analog_gain_level) /
           media::MaxWebRtcAnalogGainLevel();
  }
}

// Called on the owning sequence.
void AudioProcessor::SendLogMessage(const std::string& message) {
  log_callback_.Run(base::StringPrintf("MSAP::%s [this=0x%" PRIXPTR "]",
                                       message.c_str(),
                                       reinterpret_cast<uintptr_t>(this)));
}

// If WebRTC audio processing is used, the default output format is fixed to the
// native WebRTC processing format in order to avoid rebuffering and resampling.
// If not, then the input format is essentially preserved.
// static
AudioParameters AudioProcessor::GetDefaultOutputFormat(
    const AudioParameters& input_format,
    const AudioProcessingSettings& settings) {
  const bool need_webrtc_audio_processing =
      settings.NeedWebrtcAudioProcessing();
  const int output_sample_rate =
      need_webrtc_audio_processing ?
#if BUILDFLAG(IS_CHROMECAST)
                                   std::min(media::kAudioProcessingSampleRateHz,
                                            input_format.sample_rate())
#else
                                   media::kAudioProcessingSampleRateHz
#endif  // BUILDFLAG(IS_CHROMECAST)
                                   : input_format.sample_rate();

  media::ChannelLayout output_channel_layout;
  if (!need_webrtc_audio_processing) {
    output_channel_layout = input_format.channel_layout();
  } else if (settings.multi_channel_capture_processing) {
    // The number of output channels is equal to the number of input channels.
    // If the media stream audio processor receives stereo input it will
    // output stereo. To reduce computational complexity, APM will not perform
    // full multichannel processing unless any sink requests more than one
    // channel. If the input is multichannel but the sinks are not interested
    // in more than one channel, APM will internally downmix the signal to
    // mono and process it. The processed mono signal will then be upmixed to
    // same number of channels as the input before leaving the media stream
    // audio processor. If a sink later requests stereo, APM will start
    // performing true stereo processing. There will be no need to change the
    // output format.

    output_channel_layout = input_format.channel_layout();
  } else {
    output_channel_layout = media::CHANNEL_LAYOUT_MONO;
  }

  // webrtc::AudioProcessing requires a 10 ms chunk size. We use this native
  // size when processing is enabled. When disabled we use the same size as
  // the source if less than 10 ms.
  //
  // TODO(ajm): This conditional buffer size appears to be assuming knowledge of
  // the sink based on the source parameters. PeerConnection sinks seem to want
  // 10 ms chunks regardless, while WebAudio sinks want less, and we're assuming
  // we can identify WebAudio sinks by the input chunk size. Less fragile would
  // be to have the sink actually tell us how much it wants (as in the above
  // todo).
  int output_frames = output_sample_rate / 100;
  if (!need_webrtc_audio_processing &&
      input_format.frames_per_buffer() < output_frames) {
    output_frames = input_format.frames_per_buffer();
  }

  media::AudioParameters output_format = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, output_channel_layout,
      output_sample_rate, output_frames);
  if (output_channel_layout == media::CHANNEL_LAYOUT_DISCRETE) {
    // Explicitly set number of channels for discrete channel layouts.
    output_format.set_channels_for_discrete(input_format.channels());
  }
  return output_format;
}
}  // namespace media
