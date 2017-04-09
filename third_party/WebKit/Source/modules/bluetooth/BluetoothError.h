// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothError_h
#define BluetoothError_h

#include "platform/heap/Handle.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include "wtf/Allocator.h"

namespace blink {

// Used when generating DOMExceptions specific to each operation.
// TODO(crbug.com/684445): Add DescriptorsRetrieval.
enum class BluetoothOperation {
  kServicesRetrieval,
  kCharacteristicsRetrieval,
  kDescriptorsRetrieval,
  GATT,
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
