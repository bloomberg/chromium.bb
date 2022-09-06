// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_REGISTER_PAYLOAD_FILE_REQUEST_H_
#define ASH_SERVICES_SECURE_CHANNEL_REGISTER_PAYLOAD_FILE_REQUEST_H_

#include "ash/services/secure_channel/file_transfer_update_callback.h"

namespace ash::secure_channel {

// Captures arguments of RegisterPayloadFile() calls to help verification in
// tests.
struct RegisterPayloadFileRequest {
  RegisterPayloadFileRequest(
      int64_t payload_id,
      FileTransferUpdateCallback file_transfer_update_callback);
  // This struct is move-only so that it can be placed into containers due to
  // the |file_transfer_update_callback| field.
  RegisterPayloadFileRequest(RegisterPayloadFileRequest&&);
  RegisterPayloadFileRequest& operator=(RegisterPayloadFileRequest&&);
  ~RegisterPayloadFileRequest();

  int64_t payload_id;
  FileTransferUpdateCallback file_transfer_update_callback;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_REGISTER_PAYLOAD_FILE_REQUEST_H_
