// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_

#include <string>

#include "base/macros.h"

namespace base {
class Value;
}  // namespace base

// Calculates and validates preference value hashes.
class PrefHashCalculator {
 public:
  enum ValidationResult {
    INVALID,
    VALID,
    // Valid under a deprecated but as secure algorithm.
    VALID_SECURE_LEGACY,
  };

  // Constructs a PrefHashCalculator using |seed|, |device_id| and
  // |legacy_device_id|. The same parameters must be used in order to
  // successfully validate generated hashes. |_device_id| or |legacy_device_id|
  // may be empty.
  PrefHashCalculator(const std::string& seed,
                     const std::string& device_id,
                     const std::string& legacy_device_id);

  ~PrefHashCalculator();

  // Calculates a hash value for the supplied preference |path| and |value|.
  // |value| may be null if the preference has no value.
  std::string Calculate(const std::string& path,
                        const base::Value* value) const;

  // Validates the provided preference hash using current and legacy hashing
  // algorithms.
  ValidationResult Validate(const std::string& path,
                            const base::Value* value,
                            const std::string& hash) const;

 private:
  const std::string seed_;
  const std::string device_id_;
  const std::string legacy_device_id_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashCalculator);
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_CALCULATOR_H_
