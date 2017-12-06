// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationAvailabilityObserver_h
#define PresentationAvailabilityObserver_h

#include "platform/wtf/Vector.h"
#include "public/platform/modules/presentation/presentation.mojom-blink.h"

namespace blink {

class KURL;

// PresentationAvailabilityObserver is an interface that is implemented by
// objects that wish to be notified when there is a presentation display
// availability change for given URLs.
class PresentationAvailabilityObserver {
 public:
  virtual ~PresentationAvailabilityObserver() = default;

  virtual void AvailabilityChanged(mojom::blink::ScreenAvailability) = 0;

  virtual const Vector<KURL>& Urls() const = 0;
};

}  // namespace blink

#endif  // PresentationAvailabilityObserver_h
