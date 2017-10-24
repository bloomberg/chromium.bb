// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousInTransferResult_h
#define USBIsochronousInTransferResult_h

#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/typed_arrays/DOMDataView.h"
#include "modules/webusb/USBIsochronousInTransferPacket.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class USBIsochronousInTransferResult final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBIsochronousInTransferResult* Create(
      DOMArrayBuffer* data,
      const HeapVector<Member<USBIsochronousInTransferPacket>>& packets) {
    DOMDataView* data_view = DOMDataView::Create(data, 0, data->ByteLength());
    return new USBIsochronousInTransferResult(data_view, packets);
  }

  static USBIsochronousInTransferResult* Create(
      const HeapVector<Member<USBIsochronousInTransferPacket>>& packets,
      DOMDataView* data) {
    return new USBIsochronousInTransferResult(data, packets);
  }

  static USBIsochronousInTransferResult* Create(
      const HeapVector<Member<USBIsochronousInTransferPacket>>& packets) {
    return new USBIsochronousInTransferResult(nullptr, packets);
  }

  USBIsochronousInTransferResult(
      DOMDataView* data,
      const HeapVector<Member<USBIsochronousInTransferPacket>>& packets)
      : data_(data), packets_(packets) {}

  virtual ~USBIsochronousInTransferResult() {}

  DOMDataView* data() const { return data_; }
  const HeapVector<Member<USBIsochronousInTransferPacket>>& packets() const {
    return packets_;
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(data_);
    visitor->Trace(packets_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  Member<DOMDataView> data_;
  const HeapVector<Member<USBIsochronousInTransferPacket>> packets_;
};

}  // namespace blink

#endif  // USBIsochronousInTransferResult_h
