// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_gallery_util_service.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/services/media_gallery_util/media_parser_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

void OnMediaParserFactoryRequest(
    service_manager::ServiceKeepalive* keepalive,
    chrome::mojom::MediaParserFactoryRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<MediaParserFactory>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

MediaGalleryUtilService::MediaGalleryUtilService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

MediaGalleryUtilService::~MediaGalleryUtilService() = default;

void MediaGalleryUtilService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&OnMediaParserFactoryRequest, &service_keepalive_));
}

void MediaGalleryUtilService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
