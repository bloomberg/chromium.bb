// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousInTransferPacket_h
#define USBIsochronousInTransferPacket_h

#include "core/typed_arrays/DOMDataView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class USBIsochronousInTransferPacket final
    : public GarbageCollectedFinalized<USBIsochronousInTransferPacket>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBIsochronousInTransferPacket* Create(const String& status) {
    return new USBIsochronousInTransferPacket(status, nullptr);
  }

  static USBIsochronousInTransferPacket* Create(const String& status,
                                                DOMDataView* data) {
    return new USBIsochronousInTransferPacket(status, data);
  }

  ~USBIsochronousInTransferPacket() {}

  String status() const { return status_; }
  DOMDataView* data() const { return data_; }

  void Trace(blink::Visitor* visitor) { visitor->Trace(data_); }

 private:
  USBIsochronousInTransferPacket(const String& status, DOMDataView* data)
      : status_(status), data_(data) {}

  const String status_;
  const Member<DOMDataView> data_;
};

}  // namespace blink

#endif  // USBIsochronousInTransferPacket_h
