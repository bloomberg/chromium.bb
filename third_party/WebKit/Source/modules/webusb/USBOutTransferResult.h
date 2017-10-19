// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBOutTransferResult_h
#define USBOutTransferResult_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class USBOutTransferResult final
    : public GarbageCollectedFinalized<USBOutTransferResult>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBOutTransferResult* Create(const String& status) {
    return new USBOutTransferResult(status, 0);
  }

  static USBOutTransferResult* Create(const String& status,
                                      unsigned bytes_written) {
    return new USBOutTransferResult(status, bytes_written);
  }

  USBOutTransferResult(const String& status, unsigned bytes_written)
      : status_(status), bytes_written_(bytes_written) {}

  virtual ~USBOutTransferResult() {}

  String status() const { return status_; }
  unsigned bytesWritten() const { return bytes_written_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  const String status_;
  const unsigned bytes_written_;
};

}  // namespace blink

#endif  // USBOutTransferResult_h
