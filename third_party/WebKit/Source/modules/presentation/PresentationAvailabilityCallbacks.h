// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationAvailabilityCallbacks_h
#define PresentationAvailabilityCallbacks_h

#include "modules/presentation/PresentationPromiseProperty.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebCallbacks.h"

namespace blink {

struct WebPresentationError;

using PresentationAvailabilityCallbacks =
    WebCallbacks<bool, const WebPresentationError&>;

// PresentationAvailabilityCallback extends WebCallbacks to resolve the
// underlying promise depending on the result passed to the callback. It takes a
// WTF::Vector<KURL> in its constructor and will pass it to the
// WebAvailabilityObserver.
class PresentationAvailabilityCallbacksImpl final
    : public PresentationAvailabilityCallbacks {
 public:
  PresentationAvailabilityCallbacksImpl(PresentationAvailabilityProperty*,
                                        const WTF::Vector<KURL>&);
  ~PresentationAvailabilityCallbacksImpl() override;

  void OnSuccess(bool value) override;
  void OnError(const WebPresentationError&) override;

 private:
  Persistent<PresentationAvailabilityProperty> resolver_;
  const WTF::Vector<KURL> urls_;

  WTF_MAKE_NONCOPYABLE(PresentationAvailabilityCallbacksImpl);
};

}  // namespace blink

#endif  // PresentationAvailabilityCallbacks_h
