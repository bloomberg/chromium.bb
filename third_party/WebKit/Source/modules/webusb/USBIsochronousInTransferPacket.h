// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousInTransferPacket_h
#define USBIsochronousInTransferPacket_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMDataView.h"
#include "platform/heap/GarbageCollected.h"
#include "wtf/text/WTFString.h"

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

  DEFINE_INLINE_TRACE() { visitor->Trace(data_); }

 private:
  USBIsochronousInTransferPacket(const String& status, DOMDataView* data)
      : status_(status), data_(data) {}

  const String status_;
  const Member<DOMDataView> data_;
};

}  // namespace blink

#endif  // USBIsochronousInTransferPacket_h
