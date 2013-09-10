// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_CANCELATION_SIGNAL_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_CANCELATION_SIGNAL_H_

#include "base/synchronization/lock.h"
#include "sync/base/sync_export.h"

namespace syncer {

class CancelationObserver;

// This class is used to allow one thread to request that another abort and
// return early.
//
// The signalling thread owns this class and my call RequestStop() at any time.
// After that call, this class' IsStopRequested() will always return true.  The
// intended use case is that the task intending to support early exit will
// periodically check the value of IsStopRequested() to see if it should return
// early.
//
// The receiving task may also choose to register an CancelationObserver whose
// OnStopRequested() method will be executed on the signaller's thread when
// RequestStop() is called.  This may be used for sending an early Signal() to a
// WaitableEvent.  The registration of the handler is necessarily racy.  If
// RequestStop() is executes before TryRegisterHandler(), TryRegisterHandler()
// will not perform any registration and return false.  That function's caller
// must handle this case.
//
// This class supports only one handler, though it could easily support multiple
// observers if we found a use case for such a feature.
class SYNC_EXPORT_PRIVATE CancelationSignal {
 public:
  CancelationSignal();
  ~CancelationSignal();

  // Tries to register a handler to be invoked when RequestStop() is called.
  //
  // If RequestStop() has already been called, returns false without registering
  // the handler.  Returns true when the registration is successful.
  //
  // If the registration was successful, the handler must be unregistered with
  // UnregisterHandler before this CancelationSignal is destroyed.
  bool TryRegisterHandler(CancelationObserver* handler);

  // Unregisters the abort handler.
  void UnregisterHandler(CancelationObserver* handler);

  // Returns true if RequestStop() has been called.
  bool IsStopRequested();

  // Sets the stop_requested_ flag and calls the OnStopRequested() method of the
  // registered handler, if there is one registered at the time.
  // OnStopRequested() will be called with the |stop_requested_lock_| held.
  void RequestStop();

 private:
  // Protects all members of this class.
  base::Lock stop_requested_lock_;

  // True if RequestStop() has been invoked.
  bool stop_requested_;

  // The registered abort handler.  May be NULL.
  CancelationObserver* handler_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_CANCELATION_SIGNAL_H_
