// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_HANDLE_TYPE_H_
#define IPCZ_SRC_IPCZ_HANDLE_TYPE_H_

#include <cstdint>

#include "ipcz/ipcz.h"

namespace ipcz {

// Identifies the type of each IpczHandle attached to a parcel.
enum class HandleType : uint32_t {
  // A portal handle consumes the next available RouterDescriptor in the
  // parcel. It does not consume any other data, or any OS handles.
  kPortal = 0,

  // A box handle consumes the next available element in the parcel's
  // DriverObject array and wraps it as a Box object.
  kBox = 1,

  // TODO: Add an enumeration for relayed boxes.
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_HANDLE_TYPE_H_
