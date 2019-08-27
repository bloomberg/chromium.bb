// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {
class DOMArrayBuffer;
class NDEFRecordInit;
class NDEFMessageInit;
class NFCReaderOptions;
class NFCPushOptions;
}  // namespace blink

// Mojo type converters
namespace mojo {

template <>
struct TypeConverter<Vector<uint8_t>, WTF::String> {
  static Vector<uint8_t> Convert(const WTF::String& string);
};

template <>
struct TypeConverter<Vector<uint8_t>, blink::DOMArrayBuffer*> {
  static Vector<uint8_t> Convert(blink::DOMArrayBuffer* buffer);
};

template <>
struct TypeConverter<device::mojom::blink::NDEFRecordPtr, WTF::String> {
  static device::mojom::blink::NDEFRecordPtr Convert(const WTF::String& string);
};

template <>
struct TypeConverter<device::mojom::blink::NDEFRecordPtr,
                     blink::DOMArrayBuffer*> {
  static device::mojom::blink::NDEFRecordPtr Convert(
      blink::DOMArrayBuffer* buffer);
};

template <>
struct TypeConverter<device::mojom::blink::NDEFMessagePtr, WTF::String> {
  static device::mojom::blink::NDEFMessagePtr Convert(
      const WTF::String& string);
};

template <>
struct TypeConverter<base::Optional<Vector<uint8_t>>, blink::NDEFRecordData> {
  static base::Optional<Vector<uint8_t>> Convert(
      const blink::NDEFRecordData& value);
};

template <>
struct TypeConverter<device::mojom::blink::NDEFRecordPtr,
                     blink::NDEFRecordInit*> {
  static device::mojom::blink::NDEFRecordPtr Convert(
      const blink::NDEFRecordInit* record);
};

template <>
struct TypeConverter<device::mojom::blink::NDEFMessagePtr,
                     blink::NDEFMessageInit*> {
  static device::mojom::blink::NDEFMessagePtr Convert(
      const blink::NDEFMessageInit* message);
};

template <>
struct TypeConverter<device::mojom::blink::NDEFMessagePtr,
                     blink::DOMArrayBuffer*> {
  static device::mojom::blink::NDEFMessagePtr Convert(
      blink::DOMArrayBuffer* buffer);
};

// TODO(https://crbug.com/520391): Add a creator function like
//   static NDEFMessage* NDEFMessage::Create(const NDEFMessageSource&);
// which constructs NDEFMessage with
//   - validity check of the input NDEFMessageSource, and
//   - data serialization of each record (duplicated with code of this type
//   converter),
// after that we can convert the NDEFMessage easily to a
// device::mojom::blink::NDEFMessagePtr. This enables us to remove this type
// converter and a bunch of sub converters it depends on.
template <>
struct TypeConverter<device::mojom::blink::NDEFMessagePtr,
                     blink::NDEFMessageSource> {
  static device::mojom::blink::NDEFMessagePtr Convert(
      const blink::NDEFMessageSource& message);
};

template <>
struct TypeConverter<device::mojom::blink::NFCPushOptionsPtr,
                     const blink::NFCPushOptions*> {
  static device::mojom::blink::NFCPushOptionsPtr Convert(
      const blink::NFCPushOptions* pushOptions);
};

template <>
struct TypeConverter<device::mojom::blink::NFCReaderOptionsPtr,
                     const blink::NFCReaderOptions*> {
  static device::mojom::blink::NFCReaderOptionsPtr Convert(
      const blink::NFCReaderOptions* watchOptions);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_TYPE_CONVERTERS_H_
