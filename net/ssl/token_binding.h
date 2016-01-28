// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_TOKEN_BINDING_H_
#define NET_SSL_TOKEN_BINDING_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"

namespace net {

// Given a vector of serialized TokenBinding structs (as defined in
// draft-ietf-tokbind-protocol-04), this function combines them to form the
// serialized TokenBindingMessage struct in |*out|. This function returns a net
// error.
//
// struct {
//     TokenBinding tokenbindings<0..2^16-1>;
// } TokenBindingMessage;
Error BuildTokenBindingMessageFromTokenBindings(
    const std::vector<base::StringPiece>& token_bindings,
    std::string* out);

// Builds a TokenBinding struct with a provided TokenBindingID created from
// |*key| and a signature of |ekm| using |*key| to sign.
//
// enum {
//     rsa2048_pkcs1.5(0), rsa2048_pss(1), ecdsap256(2), (255)
// } TokenBindingKeyParameters;
//
// struct {
//     opaque modulus<1..2^16-1>;
//     opaque publicexponent<1..2^8-1>;
// } RSAPublicKey;
//
// struct {
//     opaque point <1..2^8-1>;
// } ECPoint;
//
// enum {
//     provided_token_binding(0), referred_token_binding(1), (255)
// } TokenBindingType;
//
// struct {
//     TokenBindingType tokenbinding_type;
//     TokenBindingKeyParameters key_parameters;
//     select (key_parameters) {
//         case rsa2048_pkcs1.5:
//         case rsa2048_pss:
//             RSAPublicKey rsapubkey;
//         case ecdsap256:
//             ECPoint point;
//     }
// } TokenBindingID;
//
// struct {
//     TokenBindingID tokenbindingid;
//     opaque signature<0..2^16-1>;// Signature over the exported keying
//                                 // material value
//     Extension extensions<0..2^16-1>;
// } TokenBinding;
Error BuildProvidedTokenBinding(crypto::ECPrivateKey* key,
                                const std::vector<uint8_t>& ekm,
                                std::string* out);

// Given a TokenBindingMessage, parses the first TokenBinding from it,
// extracts the ECPoint of the TokenBindingID into |*ec_point|, and extracts the
// signature of the EKM value into |*signature|. It also verifies that the first
// TokenBinding is a provided Token Binding, and that the key parameters is
// ecdsap256. This function returns whether the message was able to be parsed
// successfully.
NET_EXPORT_PRIVATE bool ParseTokenBindingMessage(
    base::StringPiece token_binding_message,
    base::StringPiece* ec_point,
    base::StringPiece* signature);

// Takes an ECPoint |ec_point| from a TokenBindingID and |signature| from a
// TokenBinding and verifies that |signature| is the signature of |ekm| using
// |ec_point| as the public key. Returns true if the signature verifies and
// false if it doesn't or some other error occurs in verification. This function
// is only provided for testing.
NET_EXPORT_PRIVATE bool VerifyEKMSignature(base::StringPiece ec_point,
                                           base::StringPiece signature,
                                           base::StringPiece ekm);

}  // namespace net

#endif  // NET_SSL_TOKEN_BINDING_H_
