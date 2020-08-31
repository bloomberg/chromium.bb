// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_audio_sink.h"

#include <algorithm>
#include <limits>

#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace {

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("WRAS::" + message);
}

}  // namespace

namespace WTF {

template <>
struct CrossThreadCopier<scoped_refptr<webrtc::AudioProcessorInterface>>
    : public CrossThreadCopierPassThrough<
          scoped_refptr<webrtc::AudioProcessorInterface>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<scoped_refptr<blink::WebRtcAudioSink::Adapter>>
    : public CrossThreadCopierPassThrough<
          scoped_refptr<blink::WebRtcAudioSink::Adapter>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

WebRtcAudioSink::WebRtcAudioSink(
    const std::string& label,
    scoped_refptr<webrtc::AudioSourceInterface> track_source,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : adapter_(
          new rtc::RefCountedObject<Adapter>(label,
                                             std::move(track_source),
                                             std::move(signaling_task_runner),
                                             std::move(main_task_runner))),
      fifo_(ConvertToBaseRepeatingCallback(
          CrossThreadBindRepeating(&WebRtcAudioSink::DeliverRebufferedAudio,
                                   CrossThreadUnretained(this)))) {
  SendLogMessage(base::StringPrintf("WebRtcAudioSink({label=%s})",
                                    adapter_->label().c_str()));
}

WebRtcAudioSink::~WebRtcAudioSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf("~WebRtcAudioSink([label=%s])",
                                    adapter_->label().c_str()));
}

void WebRtcAudioSink::SetAudioProcessor(
    scoped_refptr<webrtc::AudioProcessorInterface> processor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(processor.get());
  adapter_->set_processor(std::move(processor));
}

void WebRtcAudioSink::SetLevel(
    scoped_refptr<MediaStreamAudioLevelCalculator::Level> level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(level.get());
  adapter_->set_level(std::move(level));
}

void WebRtcAudioSink::OnEnabledChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf("OnEnabledChanged([label=%s] {enabled=%s})",
                                    adapter_->label().c_str(),
                                    (enabled ? "true" : "false")));
  PostCrossThreadTask(
      *adapter_->signaling_task_runner(), FROM_HERE,
      CrossThreadBindOnce(
          base::IgnoreResult(&WebRtcAudioSink::Adapter::set_enabled), adapter_,
          enabled));
}

void WebRtcAudioSink::OnData(const media::AudioBus& audio_bus,
                             base::TimeTicks estimated_capture_time) {
  // No thread check: OnData might be called on different threads (but not
  // concurrently).

  // TODO(crbug.com/1054769): Better to let |fifo_| handle the estimated capture
  // time and let it return a corrected interpolated capture time to
  // DeliverRebufferedAudio(). Current, similar treatment is used at different
  // places where |AudioPushFifo| is applied. So a update to |AudioPushFifo|
  // will be a joint effort, and should be carefully carried out.
  last_estimated_capture_time_ = estimated_capture_time;

  adapter_->UpdateTimestampAligner(estimated_capture_time);

  // The following will result in zero, one, or multiple synchronous calls to
  // DeliverRebufferedAudio().
  fifo_.Push(audio_bus);
}

void WebRtcAudioSink::OnSetFormat(const media::AudioParameters& params) {
  DCHECK(params.IsValid());
  SendLogMessage(base::StringPrintf("OnSetFormat([label=%s] {params=[%s]})",
                                    adapter_->label().c_str(),
                                    params.AsHumanReadableString().c_str()));
  params_ = params;
  // Make sure that our params always reflect a buffer size of 10ms.
  params_.set_frames_per_buffer(params_.sample_rate() / 100);
  fifo_.Reset(params_.frames_per_buffer());
  const int num_pcm16_data_elements =
      params_.frames_per_buffer() * params_.channels();
  interleaved_data_.reset(new int16_t[num_pcm16_data_elements]);
}

void WebRtcAudioSink::DeliverRebufferedAudio(const media::AudioBus& audio_bus,
                                             int frame_delay) {
  DCHECK(params_.IsValid());

  // TODO(miu): Why doesn't a WebRTC sink care about reference time passed to
  // OnData(), and the |frame_delay| here?  How is AV sync achieved otherwise?

  // TODO(henrika): Remove this conversion once the interface in libjingle
  // supports float vectors.
  static_assert(sizeof(interleaved_data_[0]) == 2,
                "ToInterleaved expects 2 bytes.");
  audio_bus.ToInterleaved<media::SignedInt16SampleTypeTraits>(
      audio_bus.frames(), interleaved_data_.get());

  const base::TimeTicks estimated_capture_time =
      last_estimated_capture_time_ + media::AudioTimestampHelper::FramesToTime(
                                         frame_delay, params_.sample_rate());

  adapter_->DeliverPCMToWebRtcSinks(interleaved_data_.get(),
                                    params_.sample_rate(), audio_bus.channels(),
                                    audio_bus.frames(), estimated_capture_time);
}

namespace {
// TODO(miu): MediaStreamAudioProcessor destructor requires this nonsense.
void DereferenceOnMainThread(
    const scoped_refptr<webrtc::AudioProcessorInterface>& processor) {}
}  // namespace

WebRtcAudioSink::Adapter::Adapter(
    const std::string& label,
    scoped_refptr<webrtc::AudioSourceInterface> source,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(label),
      label_(label),
      source_(std::move(source)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      main_task_runner_(std::move(main_task_runner)) {
  DCHECK(signaling_task_runner_);
  DCHECK(main_task_runner_);
  SendLogMessage(
      base::StringPrintf("Adapter::Adapter({label=%s})", label_.c_str()));
}

WebRtcAudioSink::Adapter::~Adapter() {
  SendLogMessage(
      base::StringPrintf("Adapter::~Adapter([label=%s])", label_.c_str()));
  if (audio_processor_) {
    PostCrossThreadTask(*main_task_runner_.get(), FROM_HERE,
                        CrossThreadBindOnce(&DereferenceOnMainThread,
                                            std::move(audio_processor_)));
  }
}

void WebRtcAudioSink::Adapter::DeliverPCMToWebRtcSinks(
    const int16_t* audio_data,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames,
    base::TimeTicks estimated_capture_time) {
  base::AutoLock auto_lock(lock_);

  // This use |timestamp_aligner_| to transform |estimated_capture_timestamp| to
  // rtc::TimeMicros(). See the comment at UpdateTimestampAligner() for more
  // details.
  const int64_t capture_timestamp_us = timestamp_aligner_.TranslateTimestamp(
      estimated_capture_time.since_origin().InMicroseconds());

  for (webrtc::AudioTrackSinkInterface* sink : sinks_) {
    sink->OnData(audio_data, sizeof(int16_t) * 8, sample_rate,
                 number_of_channels, number_of_frames,
                 capture_timestamp_us / rtc::kNumMicrosecsPerMillisec);
  }
}

std::string WebRtcAudioSink::Adapter::kind() const {
  return webrtc::MediaStreamTrackInterface::kAudioKind;
}

bool WebRtcAudioSink::Adapter::set_enabled(bool enable) {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksInCurrentSequence());
  SendLogMessage(
      base::StringPrintf("Adapter::set_enabled([label=%s] {enable=%s})",
                         label_.c_str(), (enable ? "true" : "false")));
  return webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>::set_enabled(
      enable);
}

void WebRtcAudioSink::Adapter::AddSink(webrtc::AudioTrackSinkInterface* sink) {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(sink);
  SendLogMessage(
      base::StringPrintf("Adapter::AddSink({label=%s})", label_.c_str()));
  base::AutoLock auto_lock(lock_);
  DCHECK(!base::Contains(sinks_, sink));
  sinks_.push_back(sink);
}

void WebRtcAudioSink::Adapter::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksInCurrentSequence());
  SendLogMessage(
      base::StringPrintf("Adapter::RemoveSink([label=%s])", label_.c_str()));
  base::AutoLock auto_lock(lock_);
  const auto it = std::find(sinks_.begin(), sinks_.end(), sink);
  if (it != sinks_.end())
    sinks_.erase(it);
}

bool WebRtcAudioSink::Adapter::GetSignalLevel(int* level) {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksInCurrentSequence());

  // |level_| is only set once, so it's safe to read without first acquiring a
  // mutex.
  if (!level_)
    return false;
  const float signal_level = level_->GetCurrent();
  DCHECK_GE(signal_level, 0.0f);
  DCHECK_LE(signal_level, 1.0f);
  // Convert from float in range [0.0,1.0] to an int in range [0,32767].
  *level = static_cast<int>(signal_level * std::numeric_limits<int16_t>::max() +
                            0.5f /* rounding to nearest int */);
  // TODO(crbug/1073391): possibly log the signal level but first check the
  // calling frequency of this method to avoid creating too much data.
  return true;
}

rtc::scoped_refptr<webrtc::AudioProcessorInterface>
WebRtcAudioSink::Adapter::GetAudioProcessor() {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksInCurrentSequence());
  return audio_processor_.get();
}

webrtc::AudioSourceInterface* WebRtcAudioSink::Adapter::GetSource() const {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksInCurrentSequence());
  return source_.get();
}

void WebRtcAudioSink::Adapter::UpdateTimestampAligner(
    base::TimeTicks capture_time) {
  // The |timestamp_aligner_| stamps an audio frame as if it is captured 'now',
  // taking rtc::TimeMicros as the reference clock. It does not provide the time
  // that the frame was originally captured, Using |timestamp_aligner_| rather
  // than calling rtc::TimeMicros is to take the advantage that it aligns its
  // output timestamps such that the time spacing in the |capture_time| is
  // maintained.
  timestamp_aligner_.TranslateTimestamp(
      capture_time.since_origin().InMicroseconds(), rtc::TimeMicros());
}

}  // namespace blink
