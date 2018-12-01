// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_keepalive.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace service_manager {

ServiceKeepalive::ServiceKeepalive(ServiceBinding* binding,
                                   base::Optional<base::TimeDelta> idle_timeout)
    : binding_(binding),
      idle_timeout_(idle_timeout),
      ref_factory_(base::BindRepeating(&ServiceKeepalive::OnRefCountZero,
                                       base::Unretained(this))) {
  ref_factory_.SetRefAddedCallback(base::BindRepeating(
      &ServiceKeepalive::OnRefAdded, base::Unretained(this)));
}

ServiceKeepalive::ServiceKeepalive(ServiceContext* context,
                                   base::Optional<base::TimeDelta> idle_timeout)
    : context_(context),
      idle_timeout_(idle_timeout),
      ref_factory_(base::BindRepeating(&ServiceKeepalive::OnRefCountZero,
                                       base::Unretained(this))) {
  ref_factory_.SetRefAddedCallback(base::BindRepeating(
      &ServiceKeepalive::OnRefAdded, base::Unretained(this)));
}

ServiceKeepalive::~ServiceKeepalive() = default;

std::unique_ptr<ServiceContextRef> ServiceKeepalive::CreateRef() {
  return ref_factory_.CreateRef();
}

bool ServiceKeepalive::HasNoRefs() {
  return ref_factory_.HasNoRefs();
}

void ServiceKeepalive::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceKeepalive::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceKeepalive::OnRefAdded() {
  if (idle_timer_) {
    idle_timer_.reset();
    for (auto& observer : observers_)
      observer.OnIdleTimeoutCancelled();
  }
}

void ServiceKeepalive::OnRefCountZero() {
  if (!idle_timeout_.has_value())
    return;
  idle_timer_.emplace();
  idle_timer_->Start(FROM_HERE, *idle_timeout_,
                     base::BindRepeating(&ServiceKeepalive::OnTimerExpired,
                                         base::Unretained(this)));
}

void ServiceKeepalive::OnTimerExpired() {
  for (auto& observer : observers_)
    observer.OnIdleTimeout();

  if (context_)
    context_->CreateQuitClosure().Run();
  else
    binding_->RequestClose();
}

}  // namespace service_manager
