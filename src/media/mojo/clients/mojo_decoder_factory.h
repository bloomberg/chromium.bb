// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/base/decoder_factory.h"

namespace media {

namespace mojom {
class InterfaceFactory;
}

class MojoDecoderFactory : public DecoderFactory {
 public:
  explicit MojoDecoderFactory(
      media::mojom::InterfaceFactory* interface_factory);
  ~MojoDecoderFactory() final;

  void CreateAudioDecoders(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) final;

  void CreateVideoDecoders(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      const RequestOverlayInfoCB& request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) final;

 private:
  media::mojom::InterfaceFactory* interface_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecoderFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_
