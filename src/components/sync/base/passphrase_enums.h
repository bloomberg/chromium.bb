// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_
#define COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_

#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

// The different states for the encryption passphrase. These control if and how
// the user should be prompted for a decryption passphrase.
// Do not re-order or delete these entries; they are used in a UMA histogram.
// Please edit SyncPassphraseType in histograms.xml if a value is added.
enum class PassphraseType {
  IMPLICIT_PASSPHRASE = 0,         // GAIA-based passphrase (deprecated).
  KEYSTORE_PASSPHRASE = 1,         // Keystore passphrase.
  FROZEN_IMPLICIT_PASSPHRASE = 2,  // Frozen GAIA passphrase.
  CUSTOM_PASSPHRASE = 3,           // User-provided passphrase.
  PASSPHRASE_TYPE_SIZE,            // The size of this enum; keep last.
};

bool IsExplicitPassphrase(PassphraseType type);
PassphraseType ProtoPassphraseTypeToEnum(
    sync_pb::NigoriSpecifics::PassphraseType type);
sync_pb::NigoriSpecifics::PassphraseType EnumPassphraseTypeToProto(
    PassphraseType type);

// Different key derivation methods. Used for deriving the encryption key from a
// user's custom passphrase.
enum class KeyDerivationMethod {
  PBKDF2_HMAC_SHA1_1003 = 0,  // PBKDF2-HMAC-SHA1 with 1003 iterations.
  SCRYPT_8192_8_11 = 1,  // scrypt with N = 2^13, r = 8, p = 11 and random salt.
  UNSUPPORTED = 2,       // Unsupported method, likely from a future version.
};

// This function accepts an integer and not KeyDerivationMethod from the proto
// in order to be able to handle new, unknown values.
KeyDerivationMethod ProtoKeyDerivationMethodToEnum(
    ::google::protobuf::int32 method);
// Note that KeyDerivationMethod::UNSUPPORTED is an invalid input for this
// function, since it corresponds to a method that we are not aware of and so
// cannot meaningfully convert. The caller is responsible for ensuring that
// KeyDerivationMethod::UNSUPPORTED is never passed to this function.
sync_pb::NigoriSpecifics::KeyDerivationMethod EnumKeyDerivationMethodToProto(
    KeyDerivationMethod method);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_
