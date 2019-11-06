// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer_pipeline.h"

#include <utility>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/cma/backend/filter_group.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_impl.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_parser.h"
#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

namespace {

const int kNumInputChannels = 2;

bool IsOutputDeviceId(const std::string& device) {
  return device == ::media::AudioDeviceDescription::kDefaultDeviceId ||
         device == ::media::AudioDeviceDescription::kCommunicationsDeviceId ||
         device == kLocalAudioDeviceId || device == kAlarmAudioDeviceId ||
         device == kPlatformAudioDeviceId /* e.g. bluetooth and aux */ ||
         device == kTtsAudioDeviceId || device == kBypassAudioDeviceId;
}

std::unique_ptr<FilterGroup> CreateFilterGroup(
    int input_channels,
    const std::string& name,
    const base::Value* filter_list,
    PostProcessingPipelineFactory* ppp_factory) {
  DCHECK(ppp_factory);
  auto pipeline =
      ppp_factory->CreatePipeline(name, filter_list, input_channels);
  return std::make_unique<FilterGroup>(input_channels, name,
                                       std::move(pipeline));
}

}  // namespace

// static
std::unique_ptr<MixerPipeline> MixerPipeline::CreateMixerPipeline(
    PostProcessingPipelineParser* config,
    PostProcessingPipelineFactory* factory) {
  std::unique_ptr<MixerPipeline> mixer_pipeline(new MixerPipeline);

  if (mixer_pipeline->BuildPipeline(config, factory)) {
    return mixer_pipeline;
  }
  return nullptr;
}

MixerPipeline::MixerPipeline() = default;
MixerPipeline::~MixerPipeline() = default;

bool MixerPipeline::BuildPipeline(PostProcessingPipelineParser* config,
                                  PostProcessingPipelineFactory* factory) {
  DCHECK(config);
  DCHECK(factory);
  int mix_group_input_channels = -1;

  // Create "stream" processor groups:
  for (auto& stream_pipeline : config->GetStreamPipelines()) {
    const base::Value* device_ids = stream_pipeline.stream_types;

    DCHECK(!device_ids->GetList().empty());
    DCHECK(device_ids->GetList()[0].is_string());
    filter_groups_.push_back(CreateFilterGroup(
        kNumInputChannels, device_ids->GetList()[0].GetString() /* name */,
        stream_pipeline.pipeline, factory));

    if (!SetGroupDeviceIds(device_ids, filter_groups_.back().get())) {
      return false;
    }

    if (mix_group_input_channels == -1) {
      mix_group_input_channels = filter_groups_.back()->GetOutputChannelCount();
    } else if (mix_group_input_channels !=
               filter_groups_.back()->GetOutputChannelCount()) {
      LOG(ERROR)
          << "All output stream mixers must have the same number of channels"
          << filter_groups_.back()->name() << " has "
          << filter_groups_.back()->GetOutputChannelCount()
          << " but others have " << mix_group_input_channels;
      return false;
    }
  }

  if (mix_group_input_channels == -1) {
    mix_group_input_channels = kNumInputChannels;
  }

  // Create "mix" processor group:
  const auto mix_pipeline = config->GetMixPipeline();
  std::unique_ptr<FilterGroup> mix_filter = CreateFilterGroup(
      mix_group_input_channels, "mix", mix_pipeline.pipeline, factory);
  for (std::unique_ptr<FilterGroup>& group : filter_groups_) {
    mix_filter->AddMixedInput(group.get());
  }
  if (!SetGroupDeviceIds(mix_pipeline.stream_types, mix_filter.get())) {
    return false;
  }
  loopback_output_group_ = mix_filter.get();
  filter_groups_.push_back(std::move(mix_filter));

  // Create "linearize" processor group:
  const auto linearize_pipeline = config->GetLinearizePipeline();
  filter_groups_.push_back(
      CreateFilterGroup(loopback_output_group_->GetOutputChannelCount(),
                        "linearize", linearize_pipeline.pipeline, factory));
  output_group_ = filter_groups_.back().get();
  output_group_->AddMixedInput(loopback_output_group_);
  if (!SetGroupDeviceIds(linearize_pipeline.stream_types, output_group_)) {
    return false;
  }

  // If no default group is provided, use the "mix" group.
  if (stream_sinks_.find(::media::AudioDeviceDescription::kDefaultDeviceId) ==
      stream_sinks_.end()) {
    stream_sinks_[::media::AudioDeviceDescription::kDefaultDeviceId] =
        loopback_output_group_;
  }

  return true;
}

bool MixerPipeline::SetGroupDeviceIds(const base::Value* ids,
                                      FilterGroup* filter_group) {
  if (!ids) {
    return true;
  }
  DCHECK(filter_group);
  DCHECK(ids->is_list());

  for (const base::Value& stream_type_val : ids->GetList()) {
    DCHECK(stream_type_val.is_string());
    const std::string& stream_type = stream_type_val.GetString();
    if (!IsOutputDeviceId(stream_type)) {
      LOG(ERROR) << stream_type
                 << " is not a stream type. Stream types are listed "
                 << "in chromecast/media/base/audio_device_ids.cc and "
                 << "media/audio/audio_device_description.cc";
      return false;
    }
    if (stream_sinks_.find(stream_type) != stream_sinks_.end()) {
      LOG(ERROR) << "Multiple instances of stream type '" << stream_type
                 << "' in cast_audio.json";
      return false;
    }
    stream_sinks_[stream_type] = filter_group;
    filter_group->AddStreamType(stream_type);
  }
  return true;
}

void MixerPipeline::Initialize(int output_samples_per_second,
                               int frames_per_write) {
  // The output group will recursively set the sample rate of all other
  // FilterGroups.
  output_group_->Initialize(output_samples_per_second, frames_per_write);
  output_group_->PrintTopology();
}

FilterGroup* MixerPipeline::GetInputGroup(const std::string& device_id) {
  auto got = stream_sinks_.find(device_id);
  if (got != stream_sinks_.end()) {
    return got->second;
  }

  return stream_sinks_[::media::AudioDeviceDescription::kDefaultDeviceId];
}

void MixerPipeline::MixAndFilter(
    int frames_per_write,
    MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay) {
  output_group_->MixAndFilter(frames_per_write, rendering_delay);
}

float* MixerPipeline::GetLoopbackOutput() {
  return loopback_output_group_->GetOutputBuffer();
}

float* MixerPipeline::GetOutput() {
  return output_group_->GetOutputBuffer();
}

int MixerPipeline::GetLoopbackChannelCount() const {
  return loopback_output_group_->GetOutputChannelCount();
}

int MixerPipeline::GetOutputChannelCount() const {
  return output_group_->GetOutputChannelCount();
}

int64_t MixerPipeline::GetPostLoopbackRenderingDelayMicroseconds() const {
  return output_group_->GetRenderingDelayMicroseconds();
}

void MixerPipeline::SetPostProcessorConfig(const std::string& name,
                                           const std::string& config) {
  for (auto&& filter_group : filter_groups_) {
    filter_group->SetPostProcessorConfig(name, config);
  }
}

void MixerPipeline::SetPlayoutChannel(int playout_channel) {
  for (auto& filter_group : filter_groups_) {
    filter_group->UpdatePlayoutChannel(playout_channel);
  }
}

}  // namespace media
}  // namespace chromecast
