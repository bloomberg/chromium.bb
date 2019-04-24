// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_PADDING_KEY_H_
#define STORAGE_BROWSER_QUOTA_PADDING_KEY_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "crypto/symmetric_key.h"

namespace storage {

// Return a copy of the default key used to calculate padding sizes.
//
// The default padding key is a singleton object whose value is randomly
// generated the first time it is requested on every browser startup. When a
// cache does not have a padding key, it is assigned the current default key.
COMPONENT_EXPORT(STORAGE_BROWSER)
std::unique_ptr<crypto::SymmetricKey> CopyDefaultPaddingKey();

// Build a key whose value is the given string.
//
// May return null if deserializing fails (e.g. if the raw key is the wrong
// size).
COMPONENT_EXPORT(STORAGE_BROWSER)
std::unique_ptr<crypto::SymmetricKey> DeserializePaddingKey(
    const std::string& raw_key);

// Get the raw value of the default padding key.
//
// Each cache stores the raw value of the key that should be used when
// calculating its padding size.
COMPONENT_EXPORT(STORAGE_BROWSER)
std::string SerializeDefaultPaddingKey();

// Reset the default key to a random value.
//
// Simulating a key change across a browser restart lets us test that padding
// calculations are using the appropriate key.
COMPONENT_EXPORT(STORAGE_BROWSER)
void ResetPaddingKeyForTesting();

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_PADDING_KEY_H_
