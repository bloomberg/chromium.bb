// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/video_decoder_proxy.h"

#include "base/logging.h"
#include "content/public/common/service_manager_connection.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

VideoDecoderProxy::VideoDecoderProxy() {
  DVLOG(1) << __func__;
}

VideoDecoderProxy::~VideoDecoderProxy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void VideoDecoderProxy::Add(media::mojom::InterfaceFactoryRequest request) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bindings_.AddBinding(this, std::move(request));
}

void VideoDecoderProxy::CreateAudioDecoder(
    media::mojom::AudioDecoderRequest request) {}

void VideoDecoderProxy::CreateVideoDecoder(
    media::mojom::VideoDecoderRequest request) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateVideoDecoder(std::move(request));
}

void VideoDecoderProxy::CreateRenderer(media::mojom::HostedRendererType type,
                                       const std::string& type_specific_id,
                                       media::mojom::RendererRequest request) {}

void VideoDecoderProxy::CreateCdm(
    const std::string& key_system,
    media::mojom::ContentDecryptionModuleRequest request) {}

void VideoDecoderProxy::CreateDecryptor(
    int cdm_id,
    media::mojom::DecryptorRequest request) {}

void VideoDecoderProxy::CreateCdmProxy(const base::Token& cdm_guid,
                                       media::mojom::CdmProxyRequest request) {}

media::mojom::InterfaceFactory* VideoDecoderProxy::GetMediaInterfaceFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!interface_factory_ptr_)
    ConnectToMediaService();

  return interface_factory_ptr_.get();
}

void VideoDecoderProxy::ConnectToMediaService() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!interface_factory_ptr_);

  media::mojom::MediaServicePtr media_service;
  // TODO(slan): Use the BrowserContext Connector instead.
  // See https://crbug.com/638950.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(media::mojom::kMediaServiceName, &media_service);

  // TODO(sandersd): Do we need to bind an empty |interfaces| implementation?
  service_manager::mojom::InterfaceProviderPtr interfaces;
  media_service->CreateInterfaceFactory(MakeRequest(&interface_factory_ptr_),
                                        std::move(interfaces));

  interface_factory_ptr_.set_connection_error_handler(
      base::BindOnce(&VideoDecoderProxy::OnMediaServiceConnectionError,
                     base::Unretained(this)));
}

void VideoDecoderProxy::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  interface_factory_ptr_.reset();
}

}  // namespace content
