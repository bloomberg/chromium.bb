// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_CAST_STREAMING_DEMUXER_H_
#define FUCHSIA_ENGINE_RENDERER_CAST_STREAMING_DEMUXER_H_

#include "fuchsia/engine/cast_streaming_session.mojom.h"
#include "media/base/demuxer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace base {
class SingleThreadTaskRunner;
}

class CastStreamingReceiver;
class CastStreamingAudioDemuxerStream;
class CastStreamingVideoDemuxerStream;

// media::Demuxer implementation for a Cast Streaming Receiver.
// This object is instantiated on the main thread, whose task runner is stored
// as |original_task_runner_|. OnStreamsInitialized() is the only method called
// on the main thread. Every other method is called on the media thread, whose
// task runner is |media_task_runner_|.
// |original_task_runner_| is used to post method calls to |receiver_|, which is
// guaranteed to outlive this object.
// TODO(crbug.com/1082821): Simplify the CastStreamingDemuxer initialization
// sequence when the CastStreamingReceiver Component has been implemented.
class CastStreamingDemuxer : public media::Demuxer {
 public:
  CastStreamingDemuxer(
      CastStreamingReceiver* receiver,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);
  ~CastStreamingDemuxer() final;

  CastStreamingDemuxer(const CastStreamingDemuxer&) = delete;
  CastStreamingDemuxer& operator=(const CastStreamingDemuxer&) = delete;

  void OnStreamsInitialized(mojom::AudioStreamInfoPtr audio_stream_info,
                            mojom::VideoStreamInfoPtr video_stream_info);

 private:
  void OnStreamsInitializedOnMediaThread(
      mojom::AudioStreamInfoPtr audio_stream_info,
      mojom::VideoStreamInfoPtr video_stream_info);

  // media::Demuxer implementation.
  std::vector<media::DemuxerStream*> GetAllStreams() final;
  std::string GetDisplayName() const final;
  void Initialize(media::DemuxerHost* host,
                  media::PipelineStatusCallback status_cb) final;
  void AbortPendingReads() final;
  void StartWaitingForSeek(base::TimeDelta seek_time) final;
  void CancelPendingSeek(base::TimeDelta seek_time) final;
  void Seek(base::TimeDelta time,
            media::PipelineStatusCallback status_cb) final;
  void Stop() final;
  base::TimeDelta GetStartTime() const final;
  base::Time GetTimelineOffset() const final;
  int64_t GetMemoryUsage() const final;
  base::Optional<media::container_names::MediaContainerName>
  GetContainerForMetrics() const final;
  void OnEnabledAudioTracksChanged(
      const std::vector<media::MediaTrack::Id>& track_ids,
      base::TimeDelta curr_time,
      TrackChangeCB change_completed_cb) final;
  void OnSelectedVideoTrackChanged(
      const std::vector<media::MediaTrack::Id>& track_ids,
      base::TimeDelta curr_time,
      TrackChangeCB change_completed_cb) final;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> original_task_runner_;
  media::DemuxerHost* host_ = nullptr;
  std::unique_ptr<CastStreamingAudioDemuxerStream> audio_stream_;
  std::unique_ptr<CastStreamingVideoDemuxerStream> video_stream_;

  // Set to true if the Demuxer was successfully initialized.
  bool was_initialization_successful_ = false;
  media::PipelineStatusCallback initialized_cb_;
  CastStreamingReceiver* const receiver_;
};

#endif  // FUCHSIA_ENGINE_RENDERER_LIBCAST_STREAMING_DEMUXER_H_
