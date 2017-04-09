// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentInstruments.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/payments/PaymentInstrument.h"

namespace blink {

PaymentInstruments::PaymentInstruments() {}

ScriptPromise PaymentInstruments::deleteInstrument(
    const String& instrument_key) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise PaymentInstruments::get(const String& instrument_key) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise PaymentInstruments::keys() {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise PaymentInstruments::has(const String& instrument_key) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise PaymentInstruments::set(const String& instrument_key,
                                      const PaymentInstrument& details) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

DEFINE_TRACE(PaymentInstruments) {}

}  // namespace blink
