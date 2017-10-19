// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBIsochronousOutTransferPacket_h
#define USBIsochronousOutTransferPacket_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class USBIsochronousOutTransferPacket final
    : public GarbageCollectedFinalized<USBIsochronousOutTransferPacket>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBIsochronousOutTransferPacket* Create(const String& status) {
    return new USBIsochronousOutTransferPacket(status, 0);
  }

  static USBIsochronousOutTransferPacket* Create(const String& status,
                                                 unsigned bytes_written) {
    return new USBIsochronousOutTransferPacket(status, bytes_written);
  }

  USBIsochronousOutTransferPacket(const String& status, unsigned bytes_written)
      : status_(status), bytes_written_(bytes_written) {}

  virtual ~USBIsochronousOutTransferPacket() {}

  String status() const { return status_; }
  unsigned bytesWritten() const { return bytes_written_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  const String status_;
  const unsigned bytes_written_;
};

}  // namespace blink

#endif  // USBIsochronousOutTransferPacket_h
