// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestEvent_h
#define PaymentRequestEvent_h

#include "modules/EventModules.h"
#include "modules/payments/PaymentAppRequestData.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class MODULES_EXPORT PaymentRequestEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentRequestEvent);

 public:
  ~PaymentRequestEvent() override;

  const AtomicString& interfaceName() const override;

  void data(PaymentAppRequestData&) const;
  void respondWith(ScriptPromise);

  DECLARE_VIRTUAL_TRACE();

 private:
  PaymentRequestEvent(const AtomicString& type,
                      const PaymentAppRequestData&,
                      WaitUntilObserver*);
};

}  // namespace blink

#endif  // PaymentRequestEvent_h
