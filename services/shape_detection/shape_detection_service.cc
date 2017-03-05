// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/shape_detection_service.h"

#include "base/macros.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/shape_detection/barcode_detection_impl.h"
#include "services/shape_detection/face_detection_provider_impl.h"

namespace shape_detection {

namespace {

void OnConnectionLost(std::unique_ptr<service_manager::ServiceContextRef> ref) {
  // No-op. This merely takes ownership of |ref| so it can be destroyed when
  // this function is invoked.
}
}

std::unique_ptr<service_manager::Service> ShapeDetectionService::Create() {
  return base::MakeUnique<ShapeDetectionService>();
}

ShapeDetectionService::ShapeDetectionService() = default;

ShapeDetectionService::~ShapeDetectionService() = default;

void ShapeDetectionService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));
}

bool ShapeDetectionService::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  // Add a reference to the service and tie it to the lifetime of the
  // InterfaceRegistry's connection.
  std::unique_ptr<service_manager::ServiceContextRef> connection_ref =
      ref_factory_->CreateRef();
  registry->AddConnectionLostClosure(
      base::Bind(&OnConnectionLost, base::Passed(&connection_ref)));
  registry->AddInterface(base::Bind(&BarcodeDetectionImpl::Create));
  registry->AddInterface(base::Bind(&FaceDetectionProviderImpl::Create));
  return true;
}

}  // namespace shape_detection
