// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_factory_holder.h"

namespace content {

MediaInterfaceFactoryHolder::MediaInterfaceFactoryHolder(
    MediaServiceGetter media_service_getter,
    FrameServicesGetter frame_services_getter)
    : media_service_getter_(std::move(media_service_getter)),
      frame_services_getter_(std::move(frame_services_getter)) {}

MediaInterfaceFactoryHolder::~MediaInterfaceFactoryHolder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

media::mojom::InterfaceFactory* MediaInterfaceFactoryHolder::Get() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!interface_factory_remote_)
    ConnectToMediaService();

  return interface_factory_remote_.get();
}

void MediaInterfaceFactoryHolder::ConnectToMediaService() {
  media_service_getter_.Run().CreateInterfaceFactory(
      interface_factory_remote_.BindNewPipeAndPassReceiver(),
      frame_services_getter_.Run());
  interface_factory_remote_.set_disconnect_handler(base::BindOnce(
      &MediaInterfaceFactoryHolder::OnMediaServiceConnectionError,
      base::Unretained(this)));
}

void MediaInterfaceFactoryHolder::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  interface_factory_remote_.reset();
}

}  // namespace content
