// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/frame_resource_coordinator.h"

#include "base/bind.h"
#include "chrome/browser/performance_manager/process_resource_coordinator.h"

namespace performance_manager {

FrameResourceCoordinator::FrameResourceCoordinator(
    PerformanceManager* performance_manager)
    : ResourceCoordinatorInterface(), weak_ptr_factory_(this) {
  resource_coordinator::CoordinationUnitID new_cu_id(
      resource_coordinator::CoordinationUnitType::kFrame,
      resource_coordinator::CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(performance_manager,
                                                 new_cu_id);
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
    resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
    const resource_coordinator::CoordinationUnitID& cu_id) {
  provider->CreateFrameCoordinationUnit(mojo::MakeRequest(&service_), cu_id);
}

void FrameResourceCoordinator::SetProcessByID(
    const resource_coordinator::CoordinationUnitID& process_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (service_)
    service_->SetProcess(process_id);
}

void FrameResourceCoordinator::AddChildFrameByID(
    const resource_coordinator::CoordinationUnitID& child_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (service_)
    service_->AddChildFrame(child_id);
}

void FrameResourceCoordinator::RemoveChildFrameByID(
    const resource_coordinator::CoordinationUnitID& child_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (service_)
    service_->RemoveChildFrame(child_id);
}

}  // namespace performance_manager
