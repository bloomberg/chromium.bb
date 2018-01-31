// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationAvailabilityCallbacks_h
#define PresentationAvailabilityCallbacks_h

#include "modules/ModulesExport.h"
#include "modules/presentation/PresentationPromiseProperty.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

// PresentationAvailabilityCallback resolves or rejects underlying promise
// depending on the availability result.
// TODO(crbug.com/749327): Consider removing this class and have
// PresentationAvailabilityState use PresentationAvailabilityProperty directly.
class MODULES_EXPORT PresentationAvailabilityCallbacks {
 public:
  PresentationAvailabilityCallbacks(PresentationAvailabilityProperty*,
                                    const WTF::Vector<KURL>&);
  virtual ~PresentationAvailabilityCallbacks();

  virtual void Resolve(bool value);
  virtual void RejectAvailabilityNotSupported();

 private:
  Persistent<PresentationAvailabilityProperty> resolver_;
  const WTF::Vector<KURL> urls_;

  WTF_MAKE_NONCOPYABLE(PresentationAvailabilityCallbacks);
};

}  // namespace blink

#endif  // PresentationAvailabilityCallbacks_h
