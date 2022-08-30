// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permission_status_listener.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/permissions/permissions.h"

namespace blink {

PermissionStatusListener* PermissionStatusListener::Create(
    Permissions& associated_permissions_object,
    ExecutionContext* execution_context,
    MojoPermissionStatus status,
    MojoPermissionDescriptor descriptor) {
  PermissionStatusListener* permission_status =
      MakeGarbageCollected<PermissionStatusListener>(
          associated_permissions_object, execution_context, status,
          std::move(descriptor));
  return permission_status;
}

PermissionStatusListener::PermissionStatusListener(
    Permissions& associated_permissions_object,
    ExecutionContext* execution_context,
    MojoPermissionStatus status,
    MojoPermissionDescriptor descriptor)
    : ExecutionContextClient(execution_context),
      status_(status),
      descriptor_(std::move(descriptor)),
      receiver_(this, execution_context) {
  associated_permissions_object.PermissionStatusObjectCreated();
}

PermissionStatusListener::~PermissionStatusListener() = default;

void PermissionStatusListener::StartListening() {
  DCHECK(!receiver_.is_bound());
  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kPermission);
  receiver_.Bind(observer.InitWithNewPipeAndPassReceiver(), task_runner);

  mojo::Remote<mojom::blink::PermissionService> service;
  ConnectToPermissionService(GetExecutionContext(),
                             service.BindNewPipeAndPassReceiver(task_runner));
  service->AddPermissionObserver(descriptor_->Clone(), status_,
                                 std::move(observer));
}

void PermissionStatusListener::StopListening() {
  receiver_.reset();
}

void PermissionStatusListener::OnPermissionStatusChange(
    MojoPermissionStatus status) {
  if (status_ == status)
    return;

  status_ = status;

  for (const auto& observer : observers_) {
    if (observer)
      observer->OnPermissionStatusChange(status);
    else
      RemoveObserver(observer);
  }
}

void PermissionStatusListener::AddObserver(Observer* observer) {
  if (observers_.IsEmpty())
    StartListening();

  observers_.insert(observer);
}

void PermissionStatusListener::RemoveObserver(Observer* observer) {
  observers_.erase(observer);

  if (observers_.IsEmpty())
    StopListening();
}

bool PermissionStatusListener::HasPendingActivity() {
  return receiver_.is_bound();
}

String PermissionStatusListener::state() const {
  return PermissionStatusToString(status_);
}

String PermissionStatusListener::name() const {
  return PermissionNameToString(descriptor_->name);
}

void PermissionStatusListener::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
  visitor->Trace(receiver_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
