// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_H_
#define MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_resource.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/base/win/dcomp_texture_wrapper.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/mojom/dcomp_surface_registry.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class MediaLog;

// MediaFoundationRendererClient lives in Renderer process talks to the
// MediaFoundationRenderer living in the MediaFoundationService (utility)
// process, using `mojo_renderer_` and `renderer_extension_`.
//
// It also manages a DCOMPTexture (via `dcomp_texture_wrapper_`) living in the
// GPU process for direct composition support. The initialization of the
// compositing path is summarized as follows:
// ```
// OnVideoNaturalSizeChange() -> CreateVideoFrame(natural_size) ->
// PaintSingleFrame() -> SwapChainPresenter::PresentDCOMPSurface() ->
// DCOMPTexture::OnUpdateParentWindowRect() -> DCOMPTexture::SendOutputRect() ->
// OnOutputRectChange() -> SetOutputRect() -> OnSetOutputRectDone()
// a) -> UpdateTextureSize(output_size), and
// b) -> renderer_extension_->GetDCOMPSurface() -> OnDCOMPSurfaceReceived() ->
//    SetDCOMPSurfaceHandle() -> OnDCOMPSurfaceHandleSet()
// ```
class MediaFoundationRendererClient
    : public Renderer,
      public RendererClient,
      public media::VideoRendererSink::RenderCallback,
      public media::mojom::MediaFoundationRendererClientExtension {
 public:
  using RendererExtension = mojom::MediaFoundationRendererExtension;
  using ClientExtension = media::mojom::MediaFoundationRendererClientExtension;

  MediaFoundationRendererClient(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      std::unique_ptr<MediaLog> media_log,
      std::unique_ptr<MojoRenderer> mojo_renderer,
      mojo::PendingRemote<RendererExtension> pending_renderer_extension,
      mojo::PendingReceiver<ClientExtension> client_extension_receiver,
      std::unique_ptr<DCOMPTextureWrapper> dcomp_texture_wrapper,
      VideoRendererSink* sink);

  MediaFoundationRendererClient(const MediaFoundationRendererClient&) = delete;
  MediaFoundationRendererClient& operator=(
      const MediaFoundationRendererClient&) = delete;

  ~MediaFoundationRendererClient() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) override;

  // RendererClient implementation.
  void OnError(PipelineStatus status) override;
  void OnEnded() override;
  void OnStatisticsUpdate(const PipelineStatistics& stats) override;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason) override;
  void OnWaiting(WaitingReason reason) override;
  void OnAudioConfigChange(const AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnVideoFrameRateChange(absl::optional<int>) override;

  // media::VideoRendererSink::RenderCallback implementation.
  scoped_refptr<media::VideoFrame> Render(
      base::TimeTicks deadline_min,
      base::TimeTicks deadline_max,
      RenderingMode rendering_mode) override;
  void OnFrameDropped() override;
  base::TimeDelta GetPreferredRenderInterval() override;

  // media::mojom::MediaFoundationRendererClientExtension
  void OnFrameAvailable(const base::UnguessableToken& frame_token,
                        const gfx::Size& size,
                        base::TimeDelta timestamp) override;
  void InitializeFramePool(
      mojom::FramePoolInitializationParametersPtr pool_info) override;

  bool IsFrameServerMode() const;

 private:
  void OnRemoteRendererInitialized(PipelineStatus status);
  void OnOutputRectChange(gfx::Rect output_rect);
  void OnSetOutputRectDone(const gfx::Size& output_size, bool success);
  void InitializeDCOMPRenderingIfNeeded();
  void OnDCOMPSurfaceReceived(
      const absl::optional<base::UnguessableToken>& token,
      const std::string& error);
  void OnDCOMPSurfaceHandleSet(bool success);
  void OnVideoFrameCreated(scoped_refptr<VideoFrame> video_frame);
  void OnCdmAttached(bool success);
  void OnConnectionError();
  void SignalMediaPlayingStateChange(bool is_playing);

  // This class is constructed on the main thread and used exclusively on the
  // media thread. Hence we store PendingRemotes so we can bind the Remotes
  // on the media task runner during/after Initialize().
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<MojoRenderer> mojo_renderer_;
  mojo::PendingRemote<RendererExtension> pending_renderer_extension_;
  std::unique_ptr<DCOMPTextureWrapper> dcomp_texture_wrapper_;
  raw_ptr<VideoRendererSink> sink_ = nullptr;

  mojo::Remote<RendererExtension> renderer_extension_;

  raw_ptr<RendererClient> client_ = nullptr;
  bool dcomp_rendering_initialized_ = false;
  gfx::Size natural_size_;  // video's native size.
  gfx::Size output_size_;   // video's output size (the on-screen video size).
  base::TimeDelta render_interval_;  // interval between the video frames
  bool output_size_updated_ = false;
  bool is_playing_ = false;
  bool has_video_ = false;
  bool media_engine_in_frame_server_mode_ = false;
  scoped_refptr<VideoFrame> dcomp_video_frame_;
  scoped_refptr<VideoFrame> next_video_frame_;

  PipelineStatusCallback init_cb_;
  raw_ptr<CdmContext> cdm_context_ = nullptr;
  CdmAttachedCB cdm_attached_cb_;

  void OnPaintComplete(const base::UnguessableToken& token);

  // The MF CDM process does not have access to the mailboxes but it creates the
  // textures. Therefore the MediaFoundationRenderer and the
  // MediaFoundationRendererClient need to have a mechanism, provided by the MF
  // CDM process, to identify which texture is ready to be sent to the video
  // sink.
  base::flat_map<base::UnguessableToken, scoped_refptr<VideoFrame>>
      video_frame_pool_;
  // Used to receive calls from the MF_CMD LPAC Utility Process.
  mojo::PendingReceiver<ClientExtension> pending_client_extension_receiver_;
  mojo::Receiver<ClientExtension> client_extension_receiver_;
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationRendererClient> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_H_
