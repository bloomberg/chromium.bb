// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMDataView;
class ExceptionState;
class ExecutionContext;
class NDEFMessage;
class NDEFRecordInit;

class MODULES_EXPORT NDEFRecord final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // |is_embedded| indicates if this record is within the context of a parent
  // record.
  static NDEFRecord* Create(const ExecutionContext*,
                            const NDEFRecordInit*,
                            ExceptionState&,
                            bool is_embedded = false);

  explicit NDEFRecord(device::mojom::NDEFRecordTypeCategory,
                      const String& record_type,
                      const String& id,
                      WTF::Vector<uint8_t>);

  // For constructing a "smart-poster", an external type or a local type record
  // whose payload is an NDEF message.
  explicit NDEFRecord(device::mojom::NDEFRecordTypeCategory,
                      const String& record_type,
                      const String& id,
                      NDEFMessage*);

  // Only for constructing "text" type record. The type category will be
  // device::mojom::NDEFRecordTypeCategory::kStandardized.
  explicit NDEFRecord(const String& id,
                      const String& encoding,
                      const String& lang,
                      WTF::Vector<uint8_t>);

  // Only for constructing "text" type record from just a text. The type
  // category will be device::mojom::NDEFRecordTypeCategory::kStandardized.
  // Called by NDEFMessage.
  explicit NDEFRecord(const ExecutionContext*, const String&);

  // Only for constructing "mime" type record. The type category will be
  // device::mojom::NDEFRecordTypeCategory::kStandardized.
  explicit NDEFRecord(const String& id,
                      const String& media_type,
                      WTF::Vector<uint8_t>);

  explicit NDEFRecord(const device::mojom::blink::NDEFRecord&);

  const String& recordType() const { return record_type_; }
  const String& mediaType() const;
  const String& id() const { return id_; }
  const String& encoding() const { return encoding_; }
  const String& lang() const { return lang_; }
  DOMDataView* data() const;
  base::Optional<HeapVector<Member<NDEFRecord>>> toRecords(
      ExceptionState& exception_state) const;

  device::mojom::NDEFRecordTypeCategory category() const { return category_; }
  const WTF::Vector<uint8_t>& payloadData() const { return payload_data_; }
  const NDEFMessage* payload_message() const { return payload_message_.Get(); }

  void Trace(Visitor*) override;

 private:
  const device::mojom::NDEFRecordTypeCategory category_;
  const String record_type_;
  const String id_;
  const String media_type_;
  const String encoding_;
  const String lang_;
  // Holds the NDEFRecord.[[PayloadData]] bytes defined at
  // https://w3c.github.io/web-nfc/#the-ndefrecord-interface.
  const WTF::Vector<uint8_t> payload_data_;
  // |payload_data_| parsed as an NDEFMessage. This field will be set for some
  // "smart-poster", external, and local type records.
  const Member<NDEFMessage> payload_message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
