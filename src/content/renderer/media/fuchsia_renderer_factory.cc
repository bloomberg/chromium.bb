// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/fuchsia_renderer_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "media/base/decoder_factory.h"
#include "media/fuchsia/audio/fuchsia_audio_renderer.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace content {

FuchsiaRendererFactory::FuchsiaRendererFactory(
    media::MediaLog* media_log,
    media::DecoderFactory* decoder_factory,
    GetGpuFactoriesCB get_gpu_factories_cb,
    blink::BrowserInterfaceBrokerProxy* interface_broker)
    : media_log_(media_log),
      decoder_factory_(decoder_factory),
      get_gpu_factories_cb_(std::move(get_gpu_factories_cb)),
      interface_broker_(interface_broker) {
  DCHECK(decoder_factory_);
}

FuchsiaRendererFactory::~FuchsiaRendererFactory() = default;

std::vector<std::unique_ptr<media::VideoDecoder>>
FuchsiaRendererFactory::CreateVideoDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::vector<std::unique_ptr<media::VideoDecoder>> video_decoders;
  decoder_factory_->CreateVideoDecoders(
      media_task_runner, gpu_factories, media_log_,
      std::move(request_overlay_info_cb), target_color_space, &video_decoders);
  return video_decoders;
}

std::unique_ptr<media::Renderer> FuchsiaRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  mojo::PendingRemote<media::mojom::FuchsiaMediaResourceProvider>
      media_resource_provider;
  interface_broker_->GetInterface(
      media_resource_provider.InitWithNewPipeAndPassReceiver());

  auto audio_renderer = std::make_unique<media::FuchsiaAudioRenderer>(
      media_log_, std::move(media_resource_provider));

  media::GpuVideoAcceleratorFactories* gpu_factories = nullptr;
  if (get_gpu_factories_cb_)
    gpu_factories = get_gpu_factories_cb_.Run();

  std::unique_ptr<media::GpuMemoryBufferVideoFramePool> gmb_pool;
  if (gpu_factories && gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames(
                           /*for_media_stream=*/false)) {
    gmb_pool = std::make_unique<media::GpuMemoryBufferVideoFramePool>(
        std::move(media_task_runner), std::move(worker_task_runner),
        gpu_factories);
  }

  std::unique_ptr<media::VideoRenderer> video_renderer(
      new media::VideoRendererImpl(
          media_task_runner, video_renderer_sink,
          // Unretained is safe here, because the RendererFactory is guaranteed
          // to outlive the RendererImpl. The RendererImpl is destroyed when
          // WMPI destructor calls pipeline_controller_.Stop() ->
          // PipelineImpl::Stop() -> RendererWrapper::Stop ->
          // RendererWrapper::DestroyRenderer(). And the RendererFactory is
          // owned by WMPI and gets called after WMPI destructor finishes.
          base::BindRepeating(&FuchsiaRendererFactory::CreateVideoDecoders,
                              base::Unretained(this), media_task_runner,
                              std::move(request_overlay_info_cb),
                              target_color_space, gpu_factories),
          /*drop_frames=*/true, media_log_, std::move(gmb_pool)));

  return std::make_unique<media::RendererImpl>(
      media_task_runner, std::move(audio_renderer), std::move(video_renderer));
}

}  // namespace content
