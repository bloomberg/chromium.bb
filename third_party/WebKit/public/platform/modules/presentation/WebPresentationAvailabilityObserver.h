// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationAvailabilityObserver_h
#define WebPresentationAvailabilityObserver_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/presentation/presentation.mojom-blink.h"

namespace blink {

class WebURL;
template <typename T>
class WebVector;

// WebPresentationAvailabilityObserver is an interface that is implemented by
// objects that wish to be notified when there is a presentation display
// availability change for given URLs.
class BLINK_PLATFORM_EXPORT WebPresentationAvailabilityObserver {
 public:
  virtual ~WebPresentationAvailabilityObserver() = default;

  virtual void AvailabilityChanged(blink::mojom::ScreenAvailability) = 0;

  virtual const WebVector<WebURL>& Urls() const = 0;
};

}  // namespace blink

#endif  // WebPresentationAvailabilityObserver_h
