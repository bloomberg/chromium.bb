// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/cancelation_signal.h"

#include "base/logging.h"
#include "sync/internal_api/public/base/cancelation_observer.h"

namespace syncer {

CancelationSignal::CancelationSignal()
  : signalled_(false),
    handler_(NULL) { }

CancelationSignal::~CancelationSignal() {
  DCHECK(!handler_);
}

bool CancelationSignal::TryRegisterHandler(CancelationObserver* handler) {
  base::AutoLock lock(signal_lock_);
  DCHECK(!handler_);

  if (signalled_)
    return false;

  handler_ = handler;
  return true;
}

void CancelationSignal::UnregisterHandler(CancelationObserver* handler) {
  base::AutoLock lock(signal_lock_);
  DCHECK_EQ(handler_, handler);
  handler_ = NULL;
}

bool CancelationSignal::IsSignalled() {
  base::AutoLock lock(signal_lock_);
  return signalled_;
}

void CancelationSignal::Signal() {
  base::AutoLock lock(signal_lock_);
  DCHECK(!signalled_);

  signalled_ = true;
  if (handler_) {
    handler_->OnSignalReceived();
  }
}

}  // namespace syncer
