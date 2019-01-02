// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_VIDEO_DECODER_PROXY_H_
#define CONTENT_BROWSER_MEDIA_VIDEO_DECODER_PROXY_H_

#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {

// This implements the media::mojom::InterfaceFactory interface for a
// RenderProcessHostImpl. Unlike MediaInterfaceProxy, only
// CreateVideoDecoder() is implemented. This allows WebRTC to create
// MojoVideoDecoder instances without a RenderFrame.
class VideoDecoderProxy : public media::mojom::InterfaceFactory {
 public:
  VideoDecoderProxy();
  ~VideoDecoderProxy() final;

  void Add(media::mojom::InterfaceFactoryRequest request);

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(media::mojom::AudioDecoderRequest request) final;
  void CreateVideoDecoder(media::mojom::VideoDecoderRequest request) final;
  void CreateRenderer(media::mojom::HostedRendererType type,
                      const std::string& type_specific_id,
                      media::mojom::RendererRequest request) final;
  void CreateCdm(const std::string& key_system,
                 media::mojom::ContentDecryptionModuleRequest request) final;
  void CreateDecryptor(int cdm_id,
                       media::mojom::DecryptorRequest request) final;
  void CreateCdmProxy(const std::string& cdm_guid,
                      media::mojom::CdmProxyRequest request) final;

 private:
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();
  void ConnectToMediaService();
  void OnMediaServiceConnectionError();

  // Connection to the remote media InterfaceFactory.
  media::mojom::InterfaceFactoryPtr interface_factory_ptr_;

  // Connections to the renderer.
  mojo::BindingSet<media::mojom::InterfaceFactory> bindings_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(VideoDecoderProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_VIDEO_DECODER_PROXY_H_
