// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousOutTransferResult_h
#define USBIsochronousOutTransferResult_h

#include "core/typed_arrays/DOMDataView.h"
#include "modules/webusb/USBIsochronousOutTransferPacket.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class USBIsochronousOutTransferResult final
    : public GarbageCollectedFinalized<USBIsochronousOutTransferResult>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBIsochronousOutTransferResult* Create(
      const HeapVector<Member<USBIsochronousOutTransferPacket>>& packets) {
    return new USBIsochronousOutTransferResult(packets);
  }

  USBIsochronousOutTransferResult(
      const HeapVector<Member<USBIsochronousOutTransferPacket>>& packets)
      : packets_(packets) {}

  virtual ~USBIsochronousOutTransferResult() {}

  const HeapVector<Member<USBIsochronousOutTransferPacket>>& packets() const {
    return packets_;
  }

  DEFINE_INLINE_TRACE() { visitor->Trace(packets_); }

 private:
  const HeapVector<Member<USBIsochronousOutTransferPacket>> packets_;
};

}  // namespace blink

#endif  // USBIsochronousOutTransferResult_h
