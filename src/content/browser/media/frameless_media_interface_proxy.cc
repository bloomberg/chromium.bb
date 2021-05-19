// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/frameless_media_interface_proxy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/media_service.h"
#include "media/base/cdm_context.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"

namespace content {

FramelessMediaInterfaceProxy::FramelessMediaInterfaceProxy() {
  DVLOG(1) << __func__;
}

FramelessMediaInterfaceProxy::~FramelessMediaInterfaceProxy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FramelessMediaInterfaceProxy::Add(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  receivers_.Add(this, std::move(receiver));
}

void FramelessMediaInterfaceProxy::CreateAudioDecoder(
    mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateAudioDecoder(std::move(receiver));
}

void FramelessMediaInterfaceProxy::CreateVideoDecoder(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateVideoDecoder(std::move(receiver));
}

void FramelessMediaInterfaceProxy::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void FramelessMediaInterfaceProxy::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if defined(OS_ANDROID)
void FramelessMediaInterfaceProxy::CreateFlingingRenderer(
    const std::string& audio_device_id,
    mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
        client_extenion,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}

void FramelessMediaInterfaceProxy::CreateMediaPlayerRenderer(
    mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
// Unimplemented method as this requires CDM and media::Renderer services with
// frame context.
void FramelessMediaInterfaceProxy::CreateMediaFoundationRenderer(
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver) {}
#endif  // defined(OS_WIN)

void FramelessMediaInterfaceProxy::CreateCdm(const std::string& key_system,
                                             const media::CdmConfig& cdm_config,
                                             CreateCdmCallback callback) {
  std::move(callback).Run(mojo::NullRemote(), nullptr, "CDM not supported");
}

media::mojom::InterfaceFactory*
FramelessMediaInterfaceProxy::GetMediaInterfaceFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!interface_factory_remote_)
    ConnectToMediaService();

  return interface_factory_remote_.get();
}

void FramelessMediaInterfaceProxy::ConnectToMediaService() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!interface_factory_remote_);

  mojo::PendingRemote<media::mojom::FrameInterfaceFactory> interfaces;
  ignore_result(interfaces.InitWithNewPipeAndPassReceiver());

  GetMediaService().CreateInterfaceFactory(
      interface_factory_remote_.BindNewPipeAndPassReceiver(),
      std::move(interfaces));
  interface_factory_remote_.set_disconnect_handler(base::BindOnce(
      &FramelessMediaInterfaceProxy::OnMediaServiceConnectionError,
      base::Unretained(this)));
}

void FramelessMediaInterfaceProxy::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  interface_factory_remote_.reset();
}

}  // namespace content
