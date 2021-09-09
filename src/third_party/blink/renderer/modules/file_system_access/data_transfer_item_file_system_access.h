// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_DATA_TRANSFER_ITEM_FILE_SYSTEM_ACCESS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_DATA_TRANSFER_ITEM_FILE_SYSTEM_ACCESS_H_

#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

class DataTransferItemFileSystemAccess {
  STATIC_ONLY(DataTransferItemFileSystemAccess);

 public:
  static ScriptPromise getAsFileSystemHandle(ScriptState*, DataTransferItem&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_DATA_TRANSFER_ITEM_FILE_SYSTEM_ACCESS_H_
