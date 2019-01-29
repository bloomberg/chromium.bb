// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/image_annotation_service.h"

#include <utility>

#include "base/bind.h"
#include "url/gurl.h"

namespace image_annotation {

// TODO(crbug.com/916420): Use real server URL.
constexpr char kServerUrl[] = "";

ImageAnnotationService::ImageAnnotationService(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : service_binding_(this, std::move(request)),
      annotator_(GURL(kServerUrl), shared_url_loader_factory) {}

ImageAnnotationService::~ImageAnnotationService() = default;

void ImageAnnotationService::OnStart() {
  registry_.AddInterface<mojom::Annotator>(base::BindRepeating(
      &Annotator::BindRequest, base::Unretained(&annotator_)));
}

// service_manager::Service:
void ImageAnnotationService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace image_annotation
