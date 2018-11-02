// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FILE_TRANSFER_HELPERS_H_
#define REMOTING_PROTOCOL_FILE_TRANSFER_HELPERS_H_

#include <cstdint>

#include "base/location.h"
#include "base/optional.h"
#include "remoting/proto/file_transfer.pb.h"

namespace remoting {
namespace protocol {

FileTransfer_Error MakeFileTransferError(
    base::Location location,
    FileTransfer_Error_Type type,
    base::Optional<std::int32_t> api_error_code = base::nullopt);

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FILE_TRANSFER_HELPERS_H_
