// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/patch_service.h"

#include "components/services/patch/file_patcher_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace patch {

PatchService::PatchService(service_manager::mojom::ServiceRequest request)
    : binding_(this, std::move(request)),
      keepalive_(&binding_, base::TimeDelta::FromSeconds(0)) {}

PatchService::~PatchService() = default;

void PatchService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (interface_name == patch::mojom::FilePatcher::Name_) {
    mojo::MakeStrongBinding(
        std::make_unique<FilePatcherImpl>(keepalive_.CreateRef()),
        patch::mojom::FilePatcherRequest(std::move(interface_pipe)));
  }
}

}  //  namespace patch
