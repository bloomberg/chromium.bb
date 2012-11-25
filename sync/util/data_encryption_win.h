// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_DATA_ENCRYPTION_WIN_H_
#define SYNC_UTIL_DATA_ENCRYPTION_WIN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace syncer {

std::vector<uint8> EncryptData(const std::string& data);
bool DecryptData(const std::vector<uint8>& in_data, std::string* out_data);

}  // namespace syncer

#endif  // SYNC_UTIL_DATA_ENCRYPTION_WIN_H_
