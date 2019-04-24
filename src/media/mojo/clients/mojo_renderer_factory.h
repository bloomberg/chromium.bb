// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_RENDERER_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_MOJO_RENDERER_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "media/mojo/interfaces/renderer.mojom.h"

namespace service_manager {
class InterfaceProvider;
}

namespace media {

class GpuVideoAcceleratorFactories;
class MojoRenderer;

// The default factory class for creating MojoRenderer.
//
// The MojoRenderer should be thought of as a pure communication layer between
// media::Pipeline and a media::Renderer in a different process.
//
// Implementors of new media::Renderer types are encouraged to create small
// wrapper factories that use MRF, rather than creating derived MojoRenderer
// types, or extending MRF. See DecryptingRendererFactory and
// MediaPlayerRendererClientFactory for examples of small wrappers around MRF.
class MojoRendererFactory : public RendererFactory {
 public:
  using GetGpuFactoriesCB = base::Callback<GpuVideoAcceleratorFactories*()>;
  using GetTypeSpecificIdCB = base::Callback<std::string()>;

  MojoRendererFactory(const GetGpuFactoriesCB& get_gpu_factories_cb,
                      media::mojom::InterfaceFactory* interface_factory);

  ~MojoRendererFactory() final;

  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      const RequestOverlayInfoCB& request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;

#if defined(OS_ANDROID)
  std::unique_ptr<MojoRenderer> CreateFlingingRenderer(
      const std::string& presentation_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      VideoRendererSink* video_renderer_sink);

  std::unique_ptr<MojoRenderer> CreateMediaPlayerRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      VideoRendererSink* video_renderer_sink);
#endif  // defined (OS_ANDROID)

 private:
  GetGpuFactoriesCB get_gpu_factories_cb_;

  // InterfaceFactory or InterfaceProvider used to create or connect to remote
  // renderer.
  media::mojom::InterfaceFactory* interface_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_RENDERER_FACTORY_H_
