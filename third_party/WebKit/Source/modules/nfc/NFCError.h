// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFCError_h
#define NFCError_h

#include "platform/wtf/Allocator.h"
#include "services/device/public/interfaces/nfc.mojom-blink.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

class NFCError {
  STATIC_ONLY(NFCError);

 public:
  // Required by CallbackPromiseAdapter
  using WebType = const device::mojom::blink::NFCErrorType&;
  static DOMException* Take(ScriptPromiseResolver*,
                            const device::mojom::blink::NFCErrorType&);
};

}  // namespace blink

#endif  // NFCError_h
