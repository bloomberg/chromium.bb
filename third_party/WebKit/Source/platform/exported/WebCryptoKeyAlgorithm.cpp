/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebCryptoKeyAlgorithm.h"

#include <memory>
#include "platform/wtf/ThreadSafeRefCounted.h"

namespace blink {

// FIXME: Remove the need for this.
WebCryptoAlgorithm CreateHash(WebCryptoAlgorithmId hash) {
  return WebCryptoAlgorithm::AdoptParamsAndCreate(hash, 0);
}

class WebCryptoKeyAlgorithmPrivate
    : public ThreadSafeRefCounted<WebCryptoKeyAlgorithmPrivate> {
 public:
  WebCryptoKeyAlgorithmPrivate(
      WebCryptoAlgorithmId id,
      std::unique_ptr<WebCryptoKeyAlgorithmParams> params)
      : id(id), params(std::move(params)) {}

  WebCryptoAlgorithmId id;
  std::unique_ptr<WebCryptoKeyAlgorithmParams> params;
};

WebCryptoKeyAlgorithm::WebCryptoKeyAlgorithm(
    WebCryptoAlgorithmId id,
    std::unique_ptr<WebCryptoKeyAlgorithmParams> params)
    : private_(WTF::AdoptRef(
          new WebCryptoKeyAlgorithmPrivate(id, std::move(params)))) {}

WebCryptoKeyAlgorithm WebCryptoKeyAlgorithm::AdoptParamsAndCreate(
    WebCryptoAlgorithmId id,
    WebCryptoKeyAlgorithmParams* params) {
  return WebCryptoKeyAlgorithm(id, WTF::WrapUnique(params));
}

WebCryptoKeyAlgorithm WebCryptoKeyAlgorithm::CreateAes(
    WebCryptoAlgorithmId id,
    unsigned short key_length_bits) {
  // FIXME: Verify that id is an AES algorithm.
  // FIXME: Move this somewhere more general.
  if (key_length_bits != 128 && key_length_bits != 192 &&
      key_length_bits != 256)
    return WebCryptoKeyAlgorithm();
  return WebCryptoKeyAlgorithm(
      id, std::make_unique<WebCryptoAesKeyAlgorithmParams>(key_length_bits));
}

WebCryptoKeyAlgorithm WebCryptoKeyAlgorithm::CreateHmac(
    WebCryptoAlgorithmId hash,
    unsigned key_length_bits) {
  if (!WebCryptoAlgorithm::IsHash(hash))
    return WebCryptoKeyAlgorithm();
  return WebCryptoKeyAlgorithm(
      kWebCryptoAlgorithmIdHmac,
      WTF::WrapUnique(new WebCryptoHmacKeyAlgorithmParams(CreateHash(hash),
                                                          key_length_bits)));
}

WebCryptoKeyAlgorithm WebCryptoKeyAlgorithm::CreateRsaHashed(
    WebCryptoAlgorithmId id,
    unsigned modulus_length_bits,
    const unsigned char* public_exponent,
    unsigned public_exponent_size,
    WebCryptoAlgorithmId hash) {
  // FIXME: Verify that id is an RSA algorithm which expects a hash
  if (!WebCryptoAlgorithm::IsHash(hash))
    return WebCryptoKeyAlgorithm();
  return WebCryptoKeyAlgorithm(
      id, WTF::WrapUnique(new WebCryptoRsaHashedKeyAlgorithmParams(
              modulus_length_bits, public_exponent, public_exponent_size,
              CreateHash(hash))));
}

WebCryptoKeyAlgorithm WebCryptoKeyAlgorithm::CreateEc(
    WebCryptoAlgorithmId id,
    WebCryptoNamedCurve named_curve) {
  return WebCryptoKeyAlgorithm(
      id, std::make_unique<WebCryptoEcKeyAlgorithmParams>(named_curve));
}

WebCryptoKeyAlgorithm WebCryptoKeyAlgorithm::CreateWithoutParams(
    WebCryptoAlgorithmId id) {
  if (!WebCryptoAlgorithm::IsKdf(id))
    return WebCryptoKeyAlgorithm();
  return WebCryptoKeyAlgorithm(id, nullptr);
}

bool WebCryptoKeyAlgorithm::IsNull() const {
  return private_.IsNull();
}

WebCryptoAlgorithmId WebCryptoKeyAlgorithm::Id() const {
  DCHECK(!IsNull());
  return private_->id;
}

WebCryptoKeyAlgorithmParamsType WebCryptoKeyAlgorithm::ParamsType() const {
  DCHECK(!IsNull());
  if (!private_->params.get())
    return kWebCryptoKeyAlgorithmParamsTypeNone;
  return private_->params->GetType();
}

WebCryptoAesKeyAlgorithmParams* WebCryptoKeyAlgorithm::AesParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoKeyAlgorithmParamsTypeAes)
    return static_cast<WebCryptoAesKeyAlgorithmParams*>(private_->params.get());
  return 0;
}

WebCryptoHmacKeyAlgorithmParams* WebCryptoKeyAlgorithm::HmacParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoKeyAlgorithmParamsTypeHmac)
    return static_cast<WebCryptoHmacKeyAlgorithmParams*>(
        private_->params.get());
  return 0;
}

WebCryptoRsaHashedKeyAlgorithmParams* WebCryptoKeyAlgorithm::RsaHashedParams()
    const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoKeyAlgorithmParamsTypeRsaHashed)
    return static_cast<WebCryptoRsaHashedKeyAlgorithmParams*>(
        private_->params.get());
  return 0;
}

WebCryptoEcKeyAlgorithmParams* WebCryptoKeyAlgorithm::EcParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoKeyAlgorithmParamsTypeEc)
    return static_cast<WebCryptoEcKeyAlgorithmParams*>(private_->params.get());
  return 0;
}

void WebCryptoKeyAlgorithm::WriteToDictionary(
    WebCryptoKeyAlgorithmDictionary* dict) const {
  DCHECK(!IsNull());
  dict->SetString("name", WebCryptoAlgorithm::LookupAlgorithmInfo(Id())->name);
  if (private_->params.get())
    private_->params.get()->WriteToDictionary(dict);
}

void WebCryptoKeyAlgorithm::Assign(const WebCryptoKeyAlgorithm& other) {
  private_ = other.private_;
}

void WebCryptoKeyAlgorithm::Reset() {
  private_.Reset();
}

}  // namespace blink
