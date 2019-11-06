// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class AudioManager;
class AudioRendererSink;
class DecoderFactory;
class RendererFactory;
class VideoRendererSink;

// Test MojoMediaClient for MediaService.
class TestMojoMediaClient : public MojoMediaClient {
 public:
  TestMojoMediaClient();
  ~TestMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize(service_manager::Connector* connector) final;
  std::unique_ptr<Renderer> CreateRenderer(
      service_manager::mojom::InterfaceProvider* host_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      const std::string& audio_device_id) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* /* host_interfaces */) final;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  std::unique_ptr<CdmProxy> CreateCdmProxy(const base::Token& cdm_guid) final;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

 private:
  std::unique_ptr<AudioManager> audio_manager_;
  std::unique_ptr<DecoderFactory> decoder_factory_;
  std::unique_ptr<RendererFactory> renderer_factory_;
  std::vector<scoped_refptr<AudioRendererSink>> audio_sinks_;
  std::vector<std::unique_ptr<VideoRendererSink>> video_sinks_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
