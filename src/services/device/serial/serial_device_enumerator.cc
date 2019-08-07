// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator.h"

#include <utility>

#include "base/unguessable_token.h"

namespace device {

SerialDeviceEnumerator::SerialDeviceEnumerator() = default;

SerialDeviceEnumerator::~SerialDeviceEnumerator() = default;

base::Optional<base::FilePath> SerialDeviceEnumerator::GetPathFromToken(
    const base::UnguessableToken& token) {
  auto it = token_path_map_.find(token);
  if (it == token_path_map_.end())
    return base::nullopt;

  return it->second;
}

const base::UnguessableToken& SerialDeviceEnumerator::GetTokenFromPath(
    const base::FilePath& path) {
  for (const auto& pair : token_path_map_) {
    if (pair.second == path)
      return pair.first;
  }
  // A new serial path.
  return token_path_map_
      .insert(std::make_pair(base::UnguessableToken::Create(), path))
      .first->first;
}

}  // namespace device
