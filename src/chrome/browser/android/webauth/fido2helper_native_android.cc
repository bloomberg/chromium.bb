// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_array.h"
#include "chrome/android/chrome_jni_headers/Fido2Helper_jni.h"
#include "components/cbor/reader.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/public_key.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

// Parses a CTAP2 attestation[1] and extracts the
// parts that the browser provides via Javascript API [2]. Called
// Fido2Helper.java when constructing the makeCredential reply.
//
// [1] https://www.w3.org/TR/webauthn/#attestation-object
// [2] https://w3c.github.io/webauthn/#sctn-public-key-easy
static jboolean JNI_Fido2Helper_ParseAttestationObject(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& jattestation_object_bytes,
    const base::android::JavaParamRef<jobject>& out_result) {
  std::vector<uint8_t> attestation_object_bytes;
  JavaByteArrayToByteVector(env, jattestation_object_bytes,
                            &attestation_object_bytes);

  base::Optional<cbor::Value> attestation_object =
      cbor::Reader::Read(attestation_object_bytes);
  if (!attestation_object || !attestation_object->is_map()) {
    return false;
  }

  const cbor::Value::MapValue& map = attestation_object->GetMap();
  // See https://www.w3.org/TR/webauthn/#generating-an-attestation-object
  cbor::Value::MapValue::const_iterator it = map.find(cbor::Value("authData"));
  if (it == map.end() || !it->second.is_bytestring()) {
    return false;
  }
  const std::vector<uint8_t>& auth_data = it->second.GetBytestring();
  // See https://www.w3.org/TR/webauthn/#sec-authenticator-data
  CBS cbs;
  CBS_init(&cbs, auth_data.data(), auth_data.size());
  uint8_t flags;
  if (  // RP ID hash.
      !CBS_skip(&cbs, 32) || !CBS_get_u8(&cbs, &flags) ||
      // Check AT flag is set.
      ((flags >> 6) & 1) == 0 ||
      // Signature counter.
      !CBS_skip(&cbs, 4)) {
    return false;
  }

  const auto result = device::AttestedCredentialData::ConsumeFromCtapResponse(
      base::span<const uint8_t>(CBS_data(&cbs), CBS_len(&cbs)));
  if (!result) {
    return false;
  }

  ScopedJavaLocalRef<jbyteArray> auth_data_java(
      ToJavaByteArray(env, auth_data));

  const device::PublicKey* pub_key = result->first.public_key();
  const base::Optional<std::vector<uint8_t>>& der_bytes(pub_key->der_bytes);
  ScopedJavaLocalRef<jbyteArray> spki_java;
  if (der_bytes) {
    spki_java.Reset(ToJavaByteArray(env, *der_bytes));
  }

  Java_AttestationObjectParts_setAll(env, out_result, auth_data_java, spki_java,
                                     pub_key->algorithm);

  return true;
}
