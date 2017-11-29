// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstallEvent_h
#define InstallEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ExtendableEvent.h"

namespace blink {

class MODULES_EXPORT InstallEvent : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static InstallEvent* Create(const AtomicString& type,
                              const ExtendableEventInit&);
  static InstallEvent* Create(const AtomicString& type,
                              const ExtendableEventInit&,
                              int event_id,
                              WaitUntilObserver*);

  ~InstallEvent() override;

  const AtomicString& InterfaceName() const override;

 protected:
  InstallEvent(const AtomicString& type, const ExtendableEventInit&);
  InstallEvent(const AtomicString& type,
               const ExtendableEventInit&,
               int event_id,
               WaitUntilObserver*);
  const int event_id_;
};

}  // namespace blink

#endif  // InstallEvent_h
