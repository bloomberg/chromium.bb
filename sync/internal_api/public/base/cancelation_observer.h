// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_CANCELATION_OBSERVER_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_CANCELATION_OBSERVER_H_

#include "sync/base/sync_export.h"

namespace syncer {

// Interface for classes that handle signals from the CancelationSignal.
class SYNC_EXPORT CancelationObserver {
 public:
  CancelationObserver();
  virtual ~CancelationObserver() = 0;

  // This may be called from a foreign thread while the CancelationSignal's lock
  // is held.  The callee should avoid performing slow or blocking operations.
  virtual void OnSignalReceived() = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_CANCELATION_OBSERVER_H_
