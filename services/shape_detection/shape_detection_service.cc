// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/shape_detection_service.h"

#include "base/macros.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/shape_detection/barcode_detection_impl.h"
#include "services/shape_detection/face_detection_provider_impl.h"
#include "services/shape_detection/text_detection_impl.h"

namespace shape_detection {

std::unique_ptr<service_manager::Service> ShapeDetectionService::Create() {
  return base::MakeUnique<ShapeDetectionService>();
}

ShapeDetectionService::ShapeDetectionService() = default;

ShapeDetectionService::~ShapeDetectionService() = default;

void ShapeDetectionService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));
  registry_.AddInterface(base::Bind(&BarcodeDetectionImpl::Create));
  registry_.AddInterface(base::Bind(&FaceDetectionProviderImpl::Create));
  registry_.AddInterface(base::Bind(&TextDetectionImpl::Create));
}

void ShapeDetectionService::OnBindInterface(
    const service_manager::ServiceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info.identity, interface_name,
                          std::move(interface_pipe));
}

}  // namespace shape_detection
