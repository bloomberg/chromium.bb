// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/audio_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_processing.h"
#include "media/webrtc/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

namespace media {
namespace {

static const int kSupportedSampleRates[] = {8000,
                                            16000,
                                            22050,
                                            32000,
                                            44100,
                                            48000
#if BUILDFLAG(IS_CHROMECAST)
                                            ,
                                            96000
#endif  // BUILDFLAG(IS_CHROMECAST)
};

using MockProcessedCaptureCallback =
    base::MockRepeatingCallback<void(const media::AudioBus& audio_bus,
                                     base::TimeTicks audio_capture_time,
                                     absl::optional<double> new_volume)>;

AudioProcessor::LogCallback LogCallbackForTesting() {
  return base::BindRepeating(
      [](base::StringPiece message) { VLOG(1) << (message); });
}

// The number of packets used for testing.
const int kNumberOfPacketsForTest = 100;

void ReadDataFromSpeechFile(char* data, int length) {
  base::FilePath file;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file));
  file = file.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(FILE_PATH_LITERAL("speech_16b_stereo_48kHz.raw"));
  DCHECK(base::PathExists(file));
  int64_t data_file_size64 = 0;
  DCHECK(base::GetFileSize(file, &data_file_size64));
  EXPECT_EQ(length, base::ReadFile(file, data, length));
  DCHECK(data_file_size64 > length);
}

void DisableDefaultSettings(AudioProcessingSettings& settings) {
  settings.echo_cancellation = false;
  settings.noise_suppression = false;
  settings.transient_noise_suppression = false;
  settings.automatic_gain_control = false;
  settings.experimental_automatic_gain_control = false;
  settings.high_pass_filter = false;
  settings.multi_channel_capture_processing = false;
  settings.stereo_mirroring = false;
  settings.force_apm_creation = false;
}

}  // namespace

class AudioProcessorTest : public ::testing::Test {
 public:
  AudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO,
                48000,
                480) {}

 protected:
  // Helper method to save duplicated code.
  static void ProcessDataAndVerifyFormat(
      AudioProcessor& audio_processor,
      MockProcessedCaptureCallback& mock_capture_callback) {
    // Read the audio data from a file.
    const media::AudioParameters& params =
        audio_processor.GetInputFormatForTesting();
    const media::AudioParameters& output_params =
        audio_processor.OutputFormat();
    const int packet_size = params.frames_per_buffer() * 2 * params.channels();
    const size_t length = packet_size * kNumberOfPacketsForTest;
    std::unique_ptr<char[]> capture_data(new char[length]);
    ReadDataFromSpeechFile(capture_data.get(), static_cast<int>(length));
    const int16_t* data_ptr =
        reinterpret_cast<const int16_t*>(capture_data.get());
    std::unique_ptr<media::AudioBus> data_bus =
        media::AudioBus::Create(params.channels(), params.frames_per_buffer());

    const base::TimeTicks input_capture_time = base::TimeTicks::Now();
    int num_preferred_channels = -1;
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
          data_ptr, data_bus->frames());

      // 1. Provide playout audio, if echo cancellation is enabled.
      const bool is_aec_enabled =
          audio_processor.has_webrtc_audio_processing() &&
          (*audio_processor.GetAudioProcessingModuleConfigForTesting())
              .echo_canceller.enabled;
      if (is_aec_enabled) {
        audio_processor.OnPlayoutData(*data_bus, params.sample_rate(),
                                      base::Milliseconds(10));
      }

      // 2. Set up expectations and process captured audio.
      EXPECT_CALL(mock_capture_callback, Run(_, _, _))
          .WillRepeatedly([&](const media::AudioBus& processed_audio,
                              base::TimeTicks audio_capture_time,
                              absl::optional<double> new_volume) {
            EXPECT_EQ(processed_audio.channels(), output_params.channels());
            EXPECT_EQ(processed_audio.frames(),
                      output_params.frames_per_buffer());
            EXPECT_EQ(audio_capture_time, input_capture_time);
          });
      audio_processor.ProcessCapturedAudio(*data_bus, input_capture_time,
                                           num_preferred_channels, 1.0, false);

      data_ptr += params.frames_per_buffer() * params.channels();

      // Test different values of num_preferred_channels.
      if (++num_preferred_channels > 5) {
        num_preferred_channels = 0;
      }
    }
  }

  void VerifyDefaultComponents(AudioProcessor& audio_processor) {
    EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());
    const webrtc::AudioProcessing::Config config =
        *audio_processor.GetAudioProcessingModuleConfigForTesting();
    EXPECT_TRUE(config.echo_canceller.enabled);
    EXPECT_TRUE(config.gain_controller1.enabled);
    EXPECT_TRUE(config.high_pass_filter.enabled);
    EXPECT_TRUE(config.noise_suppression.enabled);
    EXPECT_EQ(config.noise_suppression.level, config.noise_suppression.kHigh);
    EXPECT_FALSE(config.gain_controller1.analog_gain_controller
                     .clipping_predictor.enabled);
#if BUILDFLAG(IS_ANDROID)
    EXPECT_TRUE(config.echo_canceller.mobile_mode);
    EXPECT_EQ(config.gain_controller1.mode,
              config.gain_controller1.kFixedDigital);
#else
    EXPECT_FALSE(config.echo_canceller.mobile_mode);
    EXPECT_EQ(config.gain_controller1.mode,
              config.gain_controller1.kAdaptiveAnalog);
#endif
  }

  media::AudioParameters params_;
  MockProcessedCaptureCallback mock_capture_callback_;
  // Necessary for working with WebRTC task queues.
  base::test::TaskEnvironment task_environment_;
};

struct AudioProcessorTestMultichannelAndFormat
    : public AudioProcessorTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
  AudioParameters GetProcessorOutputParams(
      const AudioParameters& params,
      const AudioProcessingSettings& settings) {
    const bool use_input_format_for_output = std::get<1>(GetParam());
    return use_input_format_for_output
               ? params
               : AudioProcessor::GetDefaultOutputFormat(params, settings);
  }

  static std::string PrintTestName(
      const testing::TestParamInfo<ParamType>& info) {
    auto [multichannel, input_format_for_output] = info.param;
    return base::StringPrintf("MultichannelApm%sSameInputOutputFormat%s",
                              multichannel ? "True" : "False",
                              input_format_for_output ? "True" : "False");
  }
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    AudioProcessorTestMultichannelAndFormat,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    &AudioProcessorTestMultichannelAndFormat::PrintTestName);

// Test crashing with ASAN on Android. crbug.com/468762
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_WithAudioProcessing DISABLED_WithAudioProcessing
#else
#define MAYBE_WithAudioProcessing WithAudioProcessing
#endif
TEST_P(AudioProcessorTestMultichannelAndFormat, MAYBE_WithAudioProcessing) {
  AudioProcessingSettings settings{.multi_channel_capture_processing =
                                       std::get<0>(GetParam())};
  AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                 LogCallbackForTesting(), settings, params_,
                                 GetProcessorOutputParams(params_, settings));
  EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());
  VerifyDefaultComponents(audio_processor);

  ProcessDataAndVerifyFormat(audio_processor, mock_capture_callback_);
}

TEST_F(AudioProcessorTest, TurnOffDefaultConstraints) {
  AudioProcessingSettings settings;
  // Turn off the default settings and pass it to AudioProcessor.
  DisableDefaultSettings(settings);
  AudioProcessor audio_processor(
      mock_capture_callback_.Get(), LogCallbackForTesting(), settings, params_,
      AudioProcessor::GetDefaultOutputFormat(params_, settings));
  EXPECT_FALSE(audio_processor.has_webrtc_audio_processing());

  EXPECT_EQ(audio_processor.OutputFormat().sample_rate(),
            params_.sample_rate());
  EXPECT_EQ(audio_processor.OutputFormat().channels(), params_.channels());
  EXPECT_EQ(audio_processor.OutputFormat().frames_per_buffer(),
            params_.sample_rate() / 100);

  ProcessDataAndVerifyFormat(audio_processor, mock_capture_callback_);
}

// Test crashing with ASAN on Android. crbug.com/468762
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestAllSampleRates DISABLED_TestAllSampleRates
#else
#define MAYBE_TestAllSampleRates TestAllSampleRates
#endif
TEST_P(AudioProcessorTestMultichannelAndFormat, MAYBE_TestAllSampleRates) {
  AudioProcessingSettings settings{.multi_channel_capture_processing =
                                       std::get<0>(GetParam())};

  for (int sample_rate : kSupportedSampleRates) {
    SCOPED_TRACE(testing::Message() << "sample_rate=" << sample_rate);
    int buffer_size = sample_rate / 100;
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, sample_rate,
                                  buffer_size);
    AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                   LogCallbackForTesting(), settings, params,
                                   GetProcessorOutputParams(params, settings));
    EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());
    VerifyDefaultComponents(audio_processor);

    ProcessDataAndVerifyFormat(audio_processor, mock_capture_callback_);
  }
}

TEST_F(AudioProcessorTest, StartStopAecDump) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));
  {
    AudioProcessingSettings settings;
    AudioProcessor audio_processor(
        mock_capture_callback_.Get(), LogCallbackForTesting(), settings,
        params_, AudioProcessor::GetDefaultOutputFormat(params_, settings));

    // Start and stop recording.
    audio_processor.OnStartDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor.OnStopDump();

    // Start and stop a second recording.
    audio_processor.OnStartDump(base::File(
        temp_file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor.OnStopDump();
  }

  // Check that dump file is non-empty after audio processor has been
  // destroyed. Note that this test fails when compiling WebRTC
  // without protobuf support, rtc_enable_protobuf=false.
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path, &output));
  EXPECT_FALSE(output.empty());
  // The temporary file is deleted when temp_directory exits scope.
}

TEST_F(AudioProcessorTest, StartAecDumpDuringOngoingAecDump) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path_a;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path_a));
  base::FilePath temp_file_path_b;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path_b));
  {
    AudioProcessingSettings settings;
    AudioProcessor audio_processor(
        mock_capture_callback_.Get(), LogCallbackForTesting(), settings,
        params_, AudioProcessor::GetDefaultOutputFormat(params_, settings));

    // Start a recording.
    audio_processor.OnStartDump(base::File(
        temp_file_path_a, base::File::FLAG_WRITE | base::File::FLAG_OPEN));

    // Start another recording without stopping the previous one.
    audio_processor.OnStartDump(base::File(
        temp_file_path_b, base::File::FLAG_WRITE | base::File::FLAG_OPEN));
    audio_processor.OnStopDump();
  }

  // Check that dump files are non-empty after audio processor has been
  // destroyed. Note that this test fails when compiling WebRTC
  // without protobuf support, rtc_enable_protobuf=false.
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path_a, &output));
  EXPECT_FALSE(output.empty());
  ASSERT_TRUE(base::ReadFileToString(temp_file_path_b, &output));
  EXPECT_FALSE(output.empty());
  // The temporary files are deleted when temp_directory exits scope.
}

TEST_P(AudioProcessorTestMultichannelAndFormat, TestStereoAudio) {
  const bool use_multichannel_processing = std::get<0>(GetParam());

  // Construct left and right channels, and populate each channel with
  // different values.
  const int size = media::AudioBus::CalculateMemorySize(params_);
  std::unique_ptr<float, base::AlignedFreeDeleter> left_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  std::unique_ptr<float, base::AlignedFreeDeleter> right_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  std::unique_ptr<media::AudioBus> wrapper =
      media::AudioBus::CreateWrapper(params_.channels());
  wrapper->set_frames(params_.frames_per_buffer());
  wrapper->SetChannelData(0, left_channel.get());
  wrapper->SetChannelData(1, right_channel.get());
  wrapper->Zero();
  wrapper->channel(0)[0] = 1.0f;

  // Test without and with audio processing enabled.
  for (bool use_apm : {false, true}) {
    // No need to test stereo with APM if disabled.
    if (use_apm && !use_multichannel_processing) {
      continue;
    }
    SCOPED_TRACE(testing::Message() << "use_apm=" << use_apm);

    AudioProcessingSettings settings{.multi_channel_capture_processing =
                                         use_multichannel_processing};
    if (!use_apm) {
      // Turn off the audio processing.
      DisableDefaultSettings(settings);
    }
    // Turn on the stereo channels mirroring.
    settings.stereo_mirroring = true;
    AudioProcessor audio_processor(mock_capture_callback_.Get(),
                                   LogCallbackForTesting(), settings, params_,
                                   GetProcessorOutputParams(params_, settings));
    EXPECT_EQ(audio_processor.has_webrtc_audio_processing(), use_apm);
    // There's no sense in continuing if this fails.
    ASSERT_EQ(2, audio_processor.OutputFormat().channels());

    // Run the test consecutively to make sure the stereo channels are not
    // flipped back and forth.
    static const int kNumberOfPacketsForTest = 100;
    const base::TimeTicks pushed_capture_time = base::TimeTicks::Now();

    for (int num_preferred_channels = 0; num_preferred_channels <= 5;
         ++num_preferred_channels) {
      SCOPED_TRACE(testing::Message()
                   << "num_preferred_channels=" << num_preferred_channels);
      for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
        SCOPED_TRACE(testing::Message() << "packet index i=" << i);
        EXPECT_CALL(mock_capture_callback_, Run(_, _, _)).Times(1);
        // Pass audio for processing.
        audio_processor.ProcessCapturedAudio(
            *wrapper, pushed_capture_time, num_preferred_channels, 0.0, false);
      }
      // At this point, the audio processing algorithms have gotten past any
      // initial buffer silence generated from resamplers, FFTs, and whatnot.
      // Set up expectations via the mock callback:
      EXPECT_CALL(mock_capture_callback_, Run(_, _, _))
          .WillRepeatedly([&](const media::AudioBus& processed_audio,
                              base::TimeTicks audio_capture_time,
                              absl::optional<double> new_volume) {
            EXPECT_EQ(audio_capture_time, pushed_capture_time);
            if (!use_apm) {
              EXPECT_FALSE(new_volume.has_value());
            }
            if (use_apm && num_preferred_channels <= 1) {
              // Mono output. Output channels are averaged.
              EXPECT_NE(processed_audio.channel(0)[0], 0);
              EXPECT_NE(processed_audio.channel(1)[0], 0);
            } else {
              // Stereo output. Output channels are independent.
              // Note that after stereo mirroring, the _right_ channel is
              // non-zero.
              EXPECT_EQ(processed_audio.channel(0)[0], 0);
              EXPECT_NE(processed_audio.channel(1)[0], 0);
            }
          });
      // Process one more frame of audio.
      audio_processor.ProcessCapturedAudio(*wrapper, pushed_capture_time,
                                           num_preferred_channels, 0.0, false);
    }
  }
}

struct AudioProcessorDefaultOutputFormatTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, int>> {
  static std::string PrintTestName(
      const testing::TestParamInfo<ParamType>& info) {
    auto [multichannel, sample_rate] = info.param;
    return base::StringPrintf("MultichannelApm%sSampleRate%d",
                              multichannel ? "True" : "False", sample_rate);
  }
};

INSTANTIATE_TEST_CASE_P(
    /*no prefix*/,
    AudioProcessorDefaultOutputFormatTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::ValuesIn(kSupportedSampleRates)),
    &AudioProcessorDefaultOutputFormatTest::PrintTestName);

TEST_P(AudioProcessorDefaultOutputFormatTest, GetDefaultOutputFormat) {
  AudioProcessingSettings settings{.multi_channel_capture_processing =
                                       std::get<0>(GetParam())};
  const int sample_rate = std::get<1>(GetParam());

  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, sample_rate, sample_rate / 100);
  AudioParameters output_params =
      AudioProcessor::GetDefaultOutputFormat(input_params, settings);

  const int expected_sample_rate =
#if BUILDFLAG(IS_CHROMECAST)
      std::min(sample_rate, media::kAudioProcessingSampleRateHz);
#else
      media::kAudioProcessingSampleRateHz;
#endif  // BUILDFLAG(IS_CHROMECAST)
  const int expected_output_channels =
      settings.multi_channel_capture_processing ? input_params.channels() : 1;

  EXPECT_EQ(output_params.sample_rate(), expected_sample_rate);
  EXPECT_EQ(output_params.channels(), expected_output_channels);
  EXPECT_EQ(output_params.frames_per_buffer(), expected_sample_rate / 100);
}

// Ensure that discrete channel layouts do not crash with audio processing
// enabled.
TEST_F(AudioProcessorTest, DiscreteChannelLayout) {
  AudioProcessingSettings settings;

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_DISCRETE, 48000, 480);
  // Test both 1 and 2 discrete channels.
  for (int channels = 1; channels <= 2; ++channels) {
    params.set_channels_for_discrete(channels);
    AudioProcessor audio_processor(
        mock_capture_callback_.Get(), LogCallbackForTesting(), settings, params,
        AudioProcessor::GetDefaultOutputFormat(params, settings));
    EXPECT_TRUE(audio_processor.has_webrtc_audio_processing());
  }
}

// When audio processing is performed, processed audio should be delivered as
// soon as 10 ms of audio has been received.
TEST(AudioProcessorCallbackTest,
     ProcessedAudioIsDeliveredAsSoonAsPossibleWithShortBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  // Set buffer size to 4 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 4 / 1000);
  AudioProcessor audio_processor(
      mock_capture_callback.Get(), LogCallbackForTesting(), settings, params,
      AudioProcessor::GetDefaultOutputFormat(params, settings));
  ASSERT_TRUE(audio_processor.has_webrtc_audio_processing());
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 4 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 8 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 12 ms of data: Should trigger callback, with 2 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 2 + 4 ms of data: Not enough to process.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _)).Times(0);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 10 ms of data: Should trigger callback.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}

// When audio processing is performed, input containing 10 ms several times over
// should trigger a comparable number of processing callbacks.
TEST(AudioProcessorCallbackTest,
     ProcessedAudioIsDeliveredAsSoonAsPossibleWithLongBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  // Set buffer size to 35 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 35 / 1000);
  AudioProcessor audio_processor(
      mock_capture_callback.Get(), LogCallbackForTesting(), settings, params,
      AudioProcessor::GetDefaultOutputFormat(params, settings));
  ASSERT_TRUE(audio_processor.has_webrtc_audio_processing());
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 35 ms of audio --> 3 chunks of 10 ms, and 5 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(3)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 5 + 35 ms of audio --> 4 chunks of 10 ms.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(4)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}

// When no audio processing is performed, audio is delivered immediately. Note
// that unlike the other cases, unprocessed audio input of less than 10 ms is
// forwarded directly instead of collecting chunks of 10 ms.
TEST(AudioProcessorCallbackTest,
     UnprocessedAudioIsDeliveredImmediatelyWithShortBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  DisableDefaultSettings(settings);
  // Set buffer size to 4 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 4 / 1000);
  AudioProcessor audio_processor(
      mock_capture_callback.Get(), LogCallbackForTesting(), settings, params,
      AudioProcessor::GetDefaultOutputFormat(params, settings));
  ASSERT_FALSE(audio_processor.has_webrtc_audio_processing());
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 4 / 1000);
  };

  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(1)
      .WillOnce(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}

// When no audio processing is performed, audio is delivered immediately. Chunks
// greater than 10 ms are delivered in chunks of 10 ms.
TEST(AudioProcessorCallbackTest,
     UnprocessedAudioIsDeliveredImmediatelyWithLongBuffers) {
  MockProcessedCaptureCallback mock_capture_callback;
  AudioProcessingSettings settings;
  DisableDefaultSettings(settings);
  // Set buffer size to 35 ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000,
                                48000 * 35 / 1000);
  AudioProcessor audio_processor(
      mock_capture_callback.Get(), LogCallbackForTesting(), settings, params,
      AudioProcessor::GetDefaultOutputFormat(params, settings));
  ASSERT_FALSE(audio_processor.has_webrtc_audio_processing());
  int output_sample_rate = audio_processor.OutputFormat().sample_rate();
  std::unique_ptr<media::AudioBus> data_bus =
      media::AudioBus::Create(params.channels(), params.frames_per_buffer());
  data_bus->Zero();

  auto check_audio_length = [&](const media::AudioBus& processed_audio,
                                base::TimeTicks, absl::optional<double>) {
    EXPECT_EQ(processed_audio.frames(), output_sample_rate * 10 / 1000);
  };

  // 35 ms of audio --> 3 chunks of 10 ms, and 5 ms left in the processor.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(3)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
  // 5 + 35 ms of audio --> 4 chunks of 10 ms.
  EXPECT_CALL(mock_capture_callback, Run(_, _, _))
      .Times(4)
      .WillRepeatedly(check_audio_length);
  audio_processor.ProcessCapturedAudio(*data_bus, base::TimeTicks::Now(), -1,
                                       1.0, false);
}
}  // namespace media
