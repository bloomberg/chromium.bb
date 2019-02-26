// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/unzip_service.h"

#include "components/services/unzip/unzipper_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace unzip {

UnzipService::UnzipService(service_manager::mojom::ServiceRequest request)
    : binding_(this, std::move(request)),
      keepalive_(&binding_, base::TimeDelta::FromSeconds(0)) {}

UnzipService::~UnzipService() = default;

void UnzipService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (interface_name == unzip::mojom::Unzipper::Name_) {
    mojo::MakeStrongBinding(
        std::make_unique<UnzipperImpl>(keepalive_.CreateRef()),
        unzip::mojom::UnzipperRequest(std::move(interface_pipe)));
  }
}

}  //  namespace unzip
