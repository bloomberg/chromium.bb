// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBInTransferResult_h
#define USBInTransferResult_h

#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/typed_arrays/DOMDataView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class USBInTransferResult final
    : public GarbageCollectedFinalized<USBInTransferResult>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBInTransferResult* Create(const String& status,
                                     const Vector<uint8_t>& data) {
    DOMDataView* data_view = DOMDataView::Create(
        DOMArrayBuffer::Create(data.data(), data.size()), 0, data.size());
    return new USBInTransferResult(status, data_view);
  }

  static USBInTransferResult* Create(const String& status) {
    return new USBInTransferResult(status, nullptr);
  }

  static USBInTransferResult* Create(const String& status, DOMDataView* data) {
    return new USBInTransferResult(status, data);
  }

  USBInTransferResult(const String& status, DOMDataView* data)
      : status_(status), data_(data) {}

  virtual ~USBInTransferResult() {}

  String status() const { return status_; }
  DOMDataView* data() const { return data_; }

  void Trace(blink::Visitor* visitor) { visitor->Trace(data_); }

 private:
  const String status_;
  const Member<DOMDataView> data_;
};

}  // namespace blink

#endif  // USBInTransferResult_h
