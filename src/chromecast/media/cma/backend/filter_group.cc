// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/filter_group.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/mixer_input.h"
#include "chromecast/media/cma/backend/post_processing_pipeline.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/vector_math.h"

namespace chromecast {
namespace media {

FilterGroup::FilterGroup(int num_channels,
                         const std::string& name,
                         std::unique_ptr<PostProcessingPipeline> pipeline)
    : num_channels_(num_channels),
      name_(name),
      post_processing_pipeline_(std::move(pipeline)) {}

FilterGroup::~FilterGroup() = default;

void FilterGroup::AddMixedInput(FilterGroup* input) {
  mixed_inputs_.push_back(input);
  DCHECK_EQ(input->GetOutputChannelCount(), num_channels_);
}

void FilterGroup::AddStreamType(const std::string& stream_type) {
  stream_types_.push_back(stream_type);
}

void FilterGroup::Initialize(int output_samples_per_second,
                             int output_frames_per_write) {
  output_samples_per_second_ = output_samples_per_second;
  output_frames_per_write_ = output_frames_per_write;

  CHECK(post_processing_pipeline_->SetOutputSampleRate(
      output_samples_per_second_));
  input_samples_per_second_ = post_processing_pipeline_->GetInputSampleRate();
  input_frames_per_write_ = output_frames_per_write *
                            input_samples_per_second_ /
                            output_samples_per_second_;
  DCHECK_EQ(input_frames_per_write_ * output_samples_per_second_,
            output_frames_per_write_ * input_samples_per_second_)
      << "Unable to produce stable buffer sizes for resampling rate "
      << input_samples_per_second_ << " : " << output_samples_per_second_;

  for (FilterGroup* input : mixed_inputs_) {
    input->Initialize(input_samples_per_second_, input_frames_per_write_);
  }
  post_processing_pipeline_->SetContentType(content_type_);
  active_inputs_.clear();
  ResizeBuffers();

  // Run a buffer of 0's to initialize rendering delay.
  delay_seconds_ = post_processing_pipeline_->ProcessFrames(
      interleaved_.data(), input_frames_per_write_, last_volume_,
      true /* is_silence */);
}

void FilterGroup::AddInput(MixerInput* input) {
  active_inputs_.insert(input);
  if (mixed_) {
    AddTempBuffer(input->num_channels(), mixed_->frames());
  }
}

void FilterGroup::RemoveInput(MixerInput* input) {
  active_inputs_.erase(input);
}

float FilterGroup::MixAndFilter(
    int num_output_frames,
    MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay) {
  DCHECK_NE(output_samples_per_second_, 0);
  DCHECK_EQ(num_output_frames, output_frames_per_write_);

  float volume = 0.0f;
  AudioContentType content_type = static_cast<AudioContentType>(-1);

  rendering_delay.delay_microseconds += GetRenderingDelayMicroseconds();
  rendering_delay_to_output_ = rendering_delay;

  // Recursively mix inputs.
  for (auto* filter_group : mixed_inputs_) {
    volume = std::max(volume, filter_group->MixAndFilter(
                                  input_frames_per_write_, rendering_delay));
    content_type = std::max(content_type, filter_group->content_type());
  }

  // |volume| can only be 0 if no |mixed_inputs_| have data.
  // This is true because FilterGroup can only return 0 if:
  // a) It has no data and its PostProcessorPipeline is not ringing.
  //    (early return, below) or
  // b) The output volume is 0 and has NEVER been non-zero,
  //    since FilterGroup will use last_volume_ if volume is 0.
  //    In this case, there was never any data in the pipeline.
  if (active_inputs_.empty() && volume == 0.0f &&
      !post_processing_pipeline_->IsRinging()) {
    if (frames_zeroed_ < num_output_frames) {
      std::fill_n(GetOutputBuffer(),
                  num_output_frames * GetOutputChannelCount(), 0);
      frames_zeroed_ = num_output_frames;
    }
    return 0.0f;  // Output will be silence, no need to mix.
  }

  frames_zeroed_ = 0;

  // Mix InputQueues
  mixed_->ZeroFramesPartial(0, input_frames_per_write_);
  for (MixerInput* input : active_inputs_) {
    DCHECK_LT(input->num_channels(), static_cast<int>(temp_buffers_.size()));
    DCHECK(temp_buffers_[input->num_channels()]);
    ::media::AudioBus* temp = temp_buffers_[input->num_channels()].get();
    int filled =
        input->FillAudioData(input_frames_per_write_, rendering_delay, temp);
    if (filled > 0) {
      int in_c = 0;
      for (int out_c = 0; out_c < num_channels_; ++out_c) {
        input->VolumeScaleAccumulate(temp->channel(in_c), filled,
                                     mixed_->channel(out_c));
        ++in_c;
        if (in_c >= input->num_channels()) {
          in_c = 0;
        }
      }

      volume = std::max(volume, input->InstantaneousVolume());
      content_type = std::max(content_type, input->content_type());
    }
  }

  mixed_->ToInterleaved<::media::FloatSampleTypeTraits<float>>(
      input_frames_per_write_, interleaved_.data());

  // Mix FilterGroups
  for (FilterGroup* group : mixed_inputs_) {
    if (group->last_volume() > 0.0f) {
      ::media::vector_math::FMAC(group->GetOutputBuffer(), 1.0f,
                                 input_frames_per_write_ * num_channels_,
                                 interleaved_.data());
    }
  }

  if (playout_channel_selection_ != kChannelAll) {
    // Duplicate selected channel to all channels.
    float* data = interleaved_.data();
    for (int f = 0; f < input_frames_per_write_; ++f) {
      float selected = data[f * num_channels_ + playout_channel_selection_];
      for (int c = 0; c < num_channels_; ++c)
        data[f * num_channels_ + c] = selected;
    }
  }

  // Allow paused streams to "ring out" at the last valid volume.
  // If the stream volume is actually 0, this doesn't matter, since the
  // data is 0's anyway.
  bool is_silence = (volume == 0.0f);
  if (!is_silence) {
    last_volume_ = volume;
    DCHECK_NE(-1, static_cast<int>(content_type))
        << "Got frames without content type.";
    if (content_type != content_type_) {
      content_type_ = content_type;
      post_processing_pipeline_->SetContentType(content_type_);
    }
  }

  delay_seconds_ = post_processing_pipeline_->ProcessFrames(
      interleaved_.data(), input_frames_per_write_, last_volume_, is_silence);
  return last_volume_;
}

float* FilterGroup::GetOutputBuffer() {
  return post_processing_pipeline_->GetOutputBuffer();
}

int64_t FilterGroup::GetRenderingDelayMicroseconds() {
  if (output_samples_per_second_ == 0) {
    return 0;
  }
  return delay_seconds_ * base::Time::kMicrosecondsPerSecond;
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
FilterGroup::GetRenderingDelayToOutput() {
  return rendering_delay_to_output_;
}

int FilterGroup::GetOutputChannelCount() const {
  return post_processing_pipeline_->NumOutputChannels();
}

void FilterGroup::ResizeBuffers() {
  mixed_ = ::media::AudioBus::Create(num_channels_, input_frames_per_write_);
  temp_buffers_.clear();
  for (MixerInput* input : active_inputs_) {
    AddTempBuffer(input->num_channels(), input_frames_per_write_);
  }
  interleaved_.assign(input_frames_per_write_ * num_channels_, 0.0f);
}

void FilterGroup::AddTempBuffer(int num_channels, int num_frames) {
  if (static_cast<int>(temp_buffers_.size()) <= num_channels) {
    temp_buffers_.resize(num_channels + 1);
  }
  if (!temp_buffers_[num_channels]) {
    temp_buffers_[num_channels] =
        ::media::AudioBus::Create(num_channels, num_frames);
  }
}

void FilterGroup::SetPostProcessorConfig(const std::string& name,
                                         const std::string& config) {
  post_processing_pipeline_->SetPostProcessorConfig(name, config);
}

void FilterGroup::UpdatePlayoutChannel(int playout_channel) {
  if (playout_channel >= num_channels_) {
    LOG(ERROR) << "only " << num_channels_ << " present, wanted channel #"
               << playout_channel;
    return;
  }
  if (name_ == "linearize") {
    // We only do playout channel selection in the "linearize" group.
    playout_channel_selection_ = playout_channel;
  }
  post_processing_pipeline_->UpdatePlayoutChannel(playout_channel);
}

void FilterGroup::PrintTopology() const {
  std::string filter_groups;
  for (const FilterGroup* mixed_input : mixed_inputs_) {
    mixed_input->PrintTopology();
    filter_groups += "[GROUP]" + mixed_input->name() + ", ";
  }

  std::string input_groups;
  for (const std::string& stream_type : stream_types_) {
    input_groups += "[STREAM]" + stream_type + ", ";
  }

  // Trim trailing comma.
  if (!filter_groups.empty()) {
    filter_groups.resize(filter_groups.size() - 2);
  }
  if (!input_groups.empty()) {
    input_groups.resize(input_groups.size() - 2);
  }

  std::string all_inputs;
  if (filter_groups.empty()) {
    all_inputs = input_groups;
  } else if (input_groups.empty()) {
    all_inputs = filter_groups;
  } else {
    all_inputs = input_groups + " + " + filter_groups;
  }
  LOG(INFO) << all_inputs << ": " << num_channels_ << "ch@"
            << input_samples_per_second_ << "hz -> [GROUP]" << name_ << " -> "
            << GetOutputChannelCount() << "ch@" << output_samples_per_second_
            << "hz";
}

}  // namespace media
}  // namespace chromecast
