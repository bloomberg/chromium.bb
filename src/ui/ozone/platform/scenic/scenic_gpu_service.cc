// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_gpu_service.h"

#include "mojo/public/c/system/message_pipe.h"

namespace ui {

namespace {

// Fulfills an InterfaceRequest<T> using an InterfacePtr<T>.
//
// Messages queued on the InterfaceRequest's message pipe are preserved and will
// be eventually delivered to the remote end of InterfacePtr<T>'s.
//
// InterfacePtr<T> must be a brand new interface; i.e., it not have been
// previously used to send a message.
template <typename Interface>
void FulfillInterfaceRequest(
    mojo::InterfaceRequest<Interface> interface_request,
    mojo::InterfacePtr<Interface> interface_ptr) {
  MojoResult result =
      mojo::FuseMessagePipes(interface_ptr.PassInterface().PassHandle(),
                             interface_request.PassMessagePipe());
  DCHECK_EQ(result, MOJO_RESULT_OK);
}

}  // namespace

ScenicGpuService::ScenicGpuService()
    : gpu_host_request_(mojo::MakeRequest(&gpu_host_)),
      weak_ptr_factory_(this) {}

ScenicGpuService::~ScenicGpuService() {}

base::RepeatingCallback<void(mojom::ScenicGpuServiceRequest)>
ScenicGpuService::GetBinderCallback() {
  return base::BindRepeating(&ScenicGpuService::AddBinding,
                             weak_ptr_factory_.GetWeakPtr());
}

void ScenicGpuService::Initialize(mojom::ScenicGpuHostPtr gpu_host) {
  FulfillInterfaceRequest(std::move(gpu_host_request_), std::move(gpu_host));
}

void ScenicGpuService::AddBinding(mojom::ScenicGpuServiceRequest request) {
  binding_set_.AddBinding(this, std::move(request));
}

}  // namespace ui
