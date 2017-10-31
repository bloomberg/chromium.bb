// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BasicCardHelper_h
#define BasicCardHelper_h

#include "bindings/core/v8/ExceptionState.h"
#include "platform/wtf/Vector.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"

namespace blink {

class BasicCardHelper {
  STATIC_ONLY(BasicCardHelper);

 public:
  // Parse 'basic-card' data in |input| and store result in
  // |supported_networks_output| and |supported_types_output| or throw
  // exception.
  static void parseBasiccardData(
      const ScriptValue& input,
      Vector<::payments::mojom::blink::BasicCardNetwork>&
          supported_networks_output,
      Vector<::payments::mojom::blink::BasicCardType>& supported_types_output,
      ExceptionState&);

  // Check whether |input| contains 'basic-card' network names.
  static bool containsNetworkNames(const Vector<String>& input);
};

}  // namespace blink

#endif  // BasicCardHelper_h
