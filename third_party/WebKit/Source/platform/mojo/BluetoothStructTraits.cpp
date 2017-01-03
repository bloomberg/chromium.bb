// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mojo/BluetoothStructTraits.h"

#include "mojo/public/cpp/bindings/string_traits_wtf.h"

namespace mojo {

// static
bool StructTraits<bluetooth::mojom::UUIDDataView, WTF::String>::Read(
    bluetooth::mojom::UUIDDataView data,
    WTF::String* output) {
  return data.ReadUuid(output);
}

}  // namespace mojo
