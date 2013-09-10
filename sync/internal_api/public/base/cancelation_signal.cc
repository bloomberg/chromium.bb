// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/cancelation_signal.h"

#include "base/logging.h"
#include "sync/internal_api/public/base/cancelation_observer.h"

namespace syncer {

CancelationSignal::CancelationSignal()
  : stop_requested_(false),
    handler_(NULL) { }

CancelationSignal::~CancelationSignal() {
  DCHECK(!handler_);
}

bool CancelationSignal::TryRegisterHandler(CancelationObserver* handler) {
  base::AutoLock lock(stop_requested_lock_);
  DCHECK(!handler_);

  if (stop_requested_)
    return false;

  handler_ = handler;
  return true;
}

void CancelationSignal::UnregisterHandler(CancelationObserver* handler) {
  base::AutoLock lock(stop_requested_lock_);
  DCHECK_EQ(handler_, handler);
  handler_ = NULL;
}

bool CancelationSignal::IsStopRequested() {
  base::AutoLock lock(stop_requested_lock_);
  return stop_requested_;
}

void CancelationSignal::RequestStop() {
  base::AutoLock lock(stop_requested_lock_);
  DCHECK(!stop_requested_);

  stop_requested_ = true;
  if (handler_) {
    handler_->OnStopRequested();
  }
}

}  // namespace syncer
