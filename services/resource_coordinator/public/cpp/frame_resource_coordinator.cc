// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/frame_resource_coordinator.h"

#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"

namespace resource_coordinator {

FrameResourceCoordinator::FrameResourceCoordinator(
    service_manager::Connector* connector)
    : ResourceCoordinatorInterface(), weak_ptr_factory_(this) {
  CoordinationUnitID new_cu_id(CoordinationUnitType::kFrame,
                               CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(connector, new_cu_id);
}

FrameResourceCoordinator::~FrameResourceCoordinator() = default;

void FrameResourceCoordinator::SetProcess(
    const ProcessResourceCoordinator& process) {
  if (!service_ || !process.service())
    return;
  process.service()->GetID(
      base::BindOnce(&FrameResourceCoordinator::SetProcessByID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FrameResourceCoordinator::AddChildFrame(
    const FrameResourceCoordinator& child) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!service_ || !child.service())
    return;
  // We could keep the ID around ourselves, but this hop ensures that the child
  // has been created on the service-side.
  child.service()->GetID(
      base::BindOnce(&FrameResourceCoordinator::AddChildFrameByID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FrameResourceCoordinator::RemoveChildFrame(
    const FrameResourceCoordinator& child) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!service_ || !child.service())
    return;
  child.service()->GetID(
      base::BindOnce(&FrameResourceCoordinator::RemoveChildFrameByID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FrameResourceCoordinator::ConnectToService(
    mojom::CoordinationUnitProviderPtr& provider,
    const CoordinationUnitID& cu_id) {
  provider->CreateFrameCoordinationUnit(mojo::MakeRequest(&service_), cu_id);
}

void FrameResourceCoordinator::SetProcessByID(
    const CoordinationUnitID& process_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (service_)
    service_->SetProcess(process_id);
}

void FrameResourceCoordinator::AddChildFrameByID(
    const CoordinationUnitID& child_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (service_)
    service_->AddChildFrame(child_id);
}

void FrameResourceCoordinator::RemoveChildFrameByID(
    const CoordinationUnitID& child_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (service_)
    service_->RemoveChildFrame(child_id);
}

}  // namespace resource_coordinator
