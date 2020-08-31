// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_MESSAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_MESSAGE_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class NDEFMessageInit;
class NDEFRecord;
class StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

using NDEFMessageSource = StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

class MODULES_EXPORT NDEFMessage final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // |is_embedded| indicates if this message serves as payload for a parent
  // record.
  static NDEFMessage* Create(const ExecutionContext*,
                             const NDEFMessageInit*,
                             ExceptionState&,
                             bool is_embedded = false);
  static NDEFMessage* Create(const ExecutionContext*,
                             const NDEFMessageSource&,
                             ExceptionState&);
  static NDEFMessage* CreateAsPayloadOfSmartPoster(const ExecutionContext*,
                                                   const NDEFMessageInit*,
                                                   ExceptionState&);

  NDEFMessage();
  explicit NDEFMessage(const device::mojom::blink::NDEFMessage&);

  const HeapVector<Member<NDEFRecord>>& records() const;

  void Trace(Visitor*) override;

 private:
  HeapVector<Member<NDEFRecord>> records_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_MESSAGE_H_
