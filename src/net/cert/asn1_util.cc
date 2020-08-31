// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/asn1_util.h"

#include "net/cert/internal/parse_certificate.h"
#include "net/der/input.h"
#include "net/der/parser.h"

namespace net {

namespace asn1 {

namespace {

// Parses input |in| which should point to the beginning of a Certificate, and
// sets |*tbs_certificate| ready to parse the Subject. If parsing
// fails, this function returns false and |*tbs_certificate| is left in an
// undefined state.
bool SeekToSubject(der::Input in, der::Parser* tbs_certificate) {
  // From RFC 5280, section 4.1
  //    Certificate  ::=  SEQUENCE  {
  //      tbsCertificate       TBSCertificate,
  //      signatureAlgorithm   AlgorithmIdentifier,
  //      signatureValue       BIT STRING  }

  // TBSCertificate  ::=  SEQUENCE  {
  //      version         [0]  EXPLICIT Version DEFAULT v1,
  //      serialNumber         CertificateSerialNumber,
  //      signature            AlgorithmIdentifier,
  //      issuer               Name,
  //      validity             Validity,
  //      subject              Name,
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,
  //      ... }

  der::Parser parser(in);
  der::Parser certificate;
  if (!parser.ReadSequence(&certificate))
    return false;

  // We don't allow junk after the certificate.
  if (parser.HasMore())
    return false;

  if (!certificate.ReadSequence(tbs_certificate))
    return false;

  bool unused;
  if (!tbs_certificate->SkipOptionalTag(
          der::kTagConstructed | der::kTagContextSpecific | 0, &unused)) {
    return false;
  }

  // serialNumber
  if (!tbs_certificate->SkipTag(der::kInteger))
    return false;
  // signature
  if (!tbs_certificate->SkipTag(der::kSequence))
    return false;
  // issuer
  if (!tbs_certificate->SkipTag(der::kSequence))
    return false;
  // validity
  if (!tbs_certificate->SkipTag(der::kSequence))
    return false;
  return true;
}

// Parses input |in| which should point to the beginning of a Certificate, and
// sets |*tbs_certificate| ready to parse the SubjectPublicKeyInfo. If parsing
// fails, this function returns false and |*tbs_certificate| is left in an
// undefined state.
bool SeekToSPKI(der::Input in, der::Parser* tbs_certificate) {
  return SeekToSubject(in, tbs_certificate) &&
         // Skip over Subject.
         tbs_certificate->SkipTag(der::kSequence);
}

// Parses input |in| which should point to the beginning of a
// Certificate. If parsing fails, this function returns false, with
// |*extensions_present| and |*extensions_parser| left in an undefined
// state. If parsing succeeds and extensions are present, this function
// sets |*extensions_present| to true and sets |*extensions_parser|
// ready to parse the Extensions. If extensions are not present, it sets
// |*extensions_present| to false and |*extensions_parser| is left in an
// undefined state.
bool SeekToExtensions(der::Input in,
                      bool* extensions_present,
                      der::Parser* extensions_parser) {
  bool present;
  der::Parser tbs_cert_parser;
  if (!SeekToSPKI(in, &tbs_cert_parser))
    return false;

  // From RFC 5280, section 4.1
  // TBSCertificate  ::=  SEQUENCE  {
  //      ...
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,
  //      issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //      subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //      extensions      [3]  EXPLICIT Extensions OPTIONAL }

  // subjectPublicKeyInfo
  if (!tbs_cert_parser.SkipTag(der::kSequence))
    return false;
  // issuerUniqueID
  if (!tbs_cert_parser.SkipOptionalTag(
          der::kTagConstructed | der::kTagContextSpecific | 1, &present)) {
    return false;
  }
  // subjectUniqueID
  if (!tbs_cert_parser.SkipOptionalTag(
          der::kTagConstructed | der::kTagContextSpecific | 2, &present)) {
    return false;
  }

  der::Input extensions;
  if (!tbs_cert_parser.ReadOptionalTag(
          der::kTagConstructed | der::kTagContextSpecific | 3, &extensions,
          &present)) {
    return false;
  }

  if (!present) {
    *extensions_present = false;
    return true;
  }

  // Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  // Extension   ::=  SEQUENCE  {
  //      extnID      OBJECT IDENTIFIER,
  //      critical    BOOLEAN DEFAULT FALSE,
  //      extnValue   OCTET STRING }

  // |extensions| was EXPLICITly tagged, so we still need to remove the
  // ASN.1 SEQUENCE header.
  der::Parser explicit_extensions_parser(extensions);
  if (!explicit_extensions_parser.ReadSequence(extensions_parser))
    return false;

  if (explicit_extensions_parser.HasMore())
    return false;

  *extensions_present = true;
  return true;
}

// Parse a DER-encoded, X.509 certificate in |cert| and find an extension with
// the given OID. Returns false on parse error or true if the parse was
// successful. |*out_extension_present| will be true iff the extension was
// found. In the case where it was found, |*out_extension| will describe the
// extension, or is undefined on parse error or if the extension is missing.
bool ExtractExtensionWithOID(base::StringPiece cert,
                             der::Input extension_oid,
                             bool* out_extension_present,
                             ParsedExtension* out_extension) {
  der::Parser extensions;
  bool extensions_present;
  if (!SeekToExtensions(der::Input(cert), &extensions_present, &extensions))
    return false;
  if (!extensions_present) {
    *out_extension_present = false;
    return true;
  }

  while (extensions.HasMore()) {
    der::Input extension_tlv;
    if (!extensions.ReadRawTLV(&extension_tlv) ||
        !ParseExtension(extension_tlv, out_extension)) {
      return false;
    }

    if (out_extension->oid == extension_oid) {
      *out_extension_present = true;
      return true;
    }
  }

  *out_extension_present = false;
  return true;
}

bool HasExtensionWithOID(base::StringPiece cert, der::Input extension_oid) {
  bool extension_present;
  ParsedExtension extension;
  return ExtractExtensionWithOID(cert, extension_oid, &extension_present,
                                 &extension) &&
         extension_present;
}

}  // namespace

bool ExtractSubjectFromDERCert(base::StringPiece cert,
                               base::StringPiece* subject_out) {
  der::Parser parser;
  if (!SeekToSubject(der::Input(cert), &parser))
    return false;
  der::Input subject;
  if (!parser.ReadRawTLV(&subject))
    return false;
  *subject_out = subject.AsStringPiece();
  return true;
}

bool ExtractSPKIFromDERCert(base::StringPiece cert,
                            base::StringPiece* spki_out) {
  der::Parser parser;
  if (!SeekToSPKI(der::Input(cert), &parser))
    return false;
  der::Input spki;
  if (!parser.ReadRawTLV(&spki))
    return false;
  *spki_out = spki.AsStringPiece();
  return true;
}

bool ExtractSubjectPublicKeyFromSPKI(base::StringPiece spki,
                                     base::StringPiece* spk_out) {
  // From RFC 5280, Section 4.1
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //     algorithm            AlgorithmIdentifier,
  //     subjectPublicKey     BIT STRING  }
  //
  //   AlgorithmIdentifier  ::=  SEQUENCE  {
  //     algorithm               OBJECT IDENTIFIER,
  //     parameters              ANY DEFINED BY algorithm OPTIONAL  }

  // Step into SubjectPublicKeyInfo sequence.
  der::Parser parser((der::Input(spki)));
  der::Parser spki_parser;
  if (!parser.ReadSequence(&spki_parser))
    return false;

  // Step over algorithm field (a SEQUENCE).
  if (!spki_parser.SkipTag(der::kSequence))
    return false;

  // Extract the subjectPublicKey field.
  der::Input spk;
  if (!spki_parser.ReadTag(der::kBitString, &spk))
    return false;
  *spk_out = spk.AsStringPiece();
  return true;
}

bool HasTLSFeatureExtension(base::StringPiece cert) {
  // kTLSFeatureExtensionOID is the DER encoding of the OID for the
  // X.509 TLS Feature Extension.
  static const uint8_t kTLSFeatureExtensionOID[] = {0x2B, 0x06, 0x01, 0x05,
                                                    0x05, 0x07, 0x01, 0x18};

  return HasExtensionWithOID(cert, der::Input(kTLSFeatureExtensionOID));
}

bool HasCanSignHttpExchangesDraftExtension(base::StringPiece cert) {
  // kCanSignHttpExchangesDraftOid is the DER encoding of the OID for
  // canSignHttpExchangesDraft defined in:
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html
  static const uint8_t kCanSignHttpExchangesDraftOid[] = {
      0x2B, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x01, 0x16};

  bool extension_present;
  ParsedExtension extension;
  if (!ExtractExtensionWithOID(cert, der::Input(kCanSignHttpExchangesDraftOid),
                               &extension_present, &extension) ||
      !extension_present) {
    return false;
  }

  // The extension should have contents NULL.
  static const uint8_t kNull[] = {0x05, 0x00};
  return extension.value == der::Input(kNull);
}

bool ExtractSignatureAlgorithmsFromDERCert(
    base::StringPiece cert,
    base::StringPiece* cert_signature_algorithm_sequence,
    base::StringPiece* tbs_signature_algorithm_sequence) {
  // From RFC 5280, section 4.1
  //    Certificate  ::=  SEQUENCE  {
  //      tbsCertificate       TBSCertificate,
  //      signatureAlgorithm   AlgorithmIdentifier,
  //      signatureValue       BIT STRING  }

  // TBSCertificate  ::=  SEQUENCE  {
  //      version         [0]  EXPLICIT Version DEFAULT v1,
  //      serialNumber         CertificateSerialNumber,
  //      signature            AlgorithmIdentifier,
  //      issuer               Name,
  //      validity             Validity,
  //      subject              Name,
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,
  //      ... }

  der::Parser parser((der::Input(cert)));
  der::Parser certificate;
  if (!parser.ReadSequence(&certificate))
    return false;

  der::Parser tbs_certificate;
  if (!certificate.ReadSequence(&tbs_certificate))
    return false;

  bool unused;
  if (!tbs_certificate.SkipOptionalTag(
          der::kTagConstructed | der::kTagContextSpecific | 0, &unused)) {
    return false;
  }

  // serialNumber
  if (!tbs_certificate.SkipTag(der::kInteger))
    return false;
  // signature
  der::Input tbs_algorithm;
  if (!tbs_certificate.ReadRawTLV(&tbs_algorithm))
    return false;

  der::Input cert_algorithm;
  if (!certificate.ReadRawTLV(&cert_algorithm))
    return false;

  *cert_signature_algorithm_sequence = cert_algorithm.AsStringPiece();
  *tbs_signature_algorithm_sequence = tbs_algorithm.AsStringPiece();
  return true;
}

bool ExtractExtensionFromDERCert(base::StringPiece cert,
                                 base::StringPiece extension_oid,
                                 bool* out_extension_present,
                                 bool* out_extension_critical,
                                 base::StringPiece* out_contents) {
  *out_extension_present = false;
  *out_extension_critical = false;
  *out_contents = base::StringPiece();

  ParsedExtension extension;
  if (!ExtractExtensionWithOID(cert, der::Input(extension_oid),
                               out_extension_present, &extension))
    return false;
  if (!*out_extension_present)
    return true;

  *out_extension_critical = extension.critical;
  *out_contents = extension.value.AsStringPiece();
  return true;
}

} // namespace asn1

} // namespace net
