// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PIN_HASH_H_
#define REMOTING_HOST_PIN_HASH_H_

#include <string>

namespace remoting {

// Creates a Me2Me shared-secret hash, consisting of the hash method, and the
// hashed host ID and PIN.
std::string MakeHostPinHash(const std::string& host_id, const std::string& pin);

// Extracts the hash function from the given hash, uses it to calculate the
// hash of the given host ID and PIN, and compares that hash to the given hash.
// Returns true if the calculated and given hashes are equal.
bool VerifyHostPinHash(const std::string& hash,
                       const std::string& host_id,
                       const std::string& pin);

}  // namespace remoting

#endif  // REMOTING_HOST_PIN_HASH_
