// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentInstruments_h
#define PaymentInstruments_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PaymentInstrument;
class ScriptPromise;

class MODULES_EXPORT PaymentInstruments final
    : public GarbageCollected<PaymentInstruments>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(PaymentInstruments);

 public:
  PaymentInstruments();

  ScriptPromise deleteInstrument(const String& instrument_key);
  ScriptPromise get(const String& instrument_key);
  ScriptPromise keys();
  ScriptPromise has(const String& instrument_key);
  ScriptPromise set(const String& instrument_key,
                    const PaymentInstrument& details);

  DECLARE_TRACE();
};

}  // namespace blink

#endif  // PaymentInstruments_h
