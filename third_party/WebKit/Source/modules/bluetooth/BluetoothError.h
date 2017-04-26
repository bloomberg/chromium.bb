// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothError_h
#define BluetoothError_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"

namespace blink {

// Used when generating DOMExceptions specific to each operation.
enum class BluetoothOperation {
  kServicesRetrieval,
  kCharacteristicsRetrieval,
  kDescriptorsRetrieval,
  kGATT,
};

// These error codes requires detailed error messages.
enum class BluetoothErrorCode {
  kInvalidService,
  kInvalidCharacteristic,
  kInvalidDescriptor,
  kServiceNotFound,
  kCharacteristicNotFound,
  kDescriptorNotFound
};

class DOMException;

// BluetoothError is used with CallbackPromiseAdapter to receive
// WebBluetoothResult responses. See CallbackPromiseAdapter class comments.
class BluetoothError {
  STATIC_ONLY(BluetoothError);

 public:
  static DOMException* CreateNotConnectedException(BluetoothOperation);
  static DOMException* CreateDOMException(BluetoothErrorCode,
                                          const String& detailed_message);

  static DOMException* CreateDOMException(
      mojom::blink::WebBluetoothResult error);
};

}  // namespace blink

#endif  // BluetoothError_h
