// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_ERROR_H_
#define PLATFORM_BASE_ERROR_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/types/variant.h"
#include "platform/base/macros.h"

namespace openscreen {

// Represents an error returned by an OSP library operation.  An error has a
// code and an optional message.
class Error {
 public:
  // TODO(issue/65): Group/rename OSP-specific errors
  enum class Code : int8_t {
    // No error occurred.
    kNone = 0,

    // A transient condition prevented the operation from proceeding (e.g.,
    // cannot send on a non-blocking socket without blocking). This indicates
    // the caller should try again later.
    kAgain = -1,

    // CBOR errors.
    kCborParsing = 1,
    kCborEncoding,
    kCborIncompleteMessage,
    kCborInvalidResponseId,
    kCborInvalidMessage,

    // Presentation start errors.
    kNoAvailableReceivers,
    kRequestCancelled,
    kNoPresentationFound,
    kPreviousStartInProgress,
    kUnknownStartError,
    kUnknownRequestId,

    kAddressInUse,
    kDomainNameTooLong,
    kDomainNameLabelTooLong,

    kIOFailure,
    kInitializationFailure,
    kInvalidIPV4Address,
    kInvalidIPV6Address,
    kConnectionFailed,

    kSocketOptionSettingFailure,
    kSocketAcceptFailure,
    kSocketBindFailure,
    kSocketClosedFailure,
    kSocketConnectFailure,
    kSocketInvalidState,
    kSocketListenFailure,
    kSocketReadFailure,
    kSocketSendFailure,

    kMdnsRegisterFailure,

    kParseError,
    kUnknownMessageType,

    kNoActiveConnection,
    kAlreadyClosed,
    kInvalidConnectionState,
    kNoStartedPresentation,
    kPresentationAlreadyStarted,

    kJsonParseError,
    kJsonWriteError,

    // OpenSSL errors.

    // Was unable to generate an RSA key.
    kRSAKeyGenerationFailure,

    // Was unable to initialize an EVP_PKEY type.
    kEVPInitializationError,

    // Was unable to generate a certificate.
    kCertificateCreationError,

    // Certificate failed validation.
    kCertificateValidationError,

    // Failed to produce a hashing digest.
    kSha256HashFailure,

    // A non-recoverable SSL library error has occurred.
    kFatalSSLError,
    kFileLoadFailure,

    // Cast certificate errors.

    // Certificates were not provided for verification.
    kErrCertsMissing,

    // The certificates provided could not be parsed.
    kErrCertsParse,

    // Key usage is missing or is not set to Digital Signature.
    // This error could also be thrown if the CN is missing.
    kErrCertsRestrictions,

    // The current date is before the notBefore date or after the notAfter date.
    kErrCertsDateInvalid,

    // The certificate failed to chain to a trusted root.
    kErrCertsVerifyGeneric,

    // The CRL is missing or failed to verify.
    kErrCrlInvalid,

    // One of the certificates in the chain is revoked.
    kErrCertsRevoked,

    // The pathlen constraint of the root certificate was exceeded.
    kErrCertsPathlen,

    // Cast authentication errors.
    kCastV2PeerCertEmpty,
    kCastV2WrongPayloadType,
    kCastV2NoPayload,
    kCastV2PayloadParsingFailed,
    kCastV2MessageError,
    kCastV2NoResponse,
    kCastV2FingerprintNotFound,
    kCastV2CertNotSignedByTrustedCa,
    kCastV2CannotExtractPublicKey,
    kCastV2SignedBlobsMismatch,
    kCastV2TlsCertValidityPeriodTooLong,
    kCastV2TlsCertValidStartDateInFuture,
    kCastV2TlsCertExpired,
    kCastV2SenderNonceMismatch,
    kCastV2DigestUnsupported,
    kCastV2SignatureEmpty,

    // Generic errors.
    kUnknownError,
    kNotImplemented,
    kInsufficientBuffer,
    kParameterInvalid,
    kParameterOutOfRange,
    kParameterNullPointer,
    kIndexOutOfBounds,
    kItemAlreadyExists,
    kItemNotFound,
    kOperationInvalid,
    kOperationCancelled,
  };

  Error();
  Error(const Error& error);
  Error(Error&& error) noexcept;

  Error(Code code);  // NOLINT
  Error(Code code, const std::string& message);
  Error(Code code, std::string&& message);
  ~Error();

  Error& operator=(const Error& other);
  Error& operator=(Error&& other);
  bool operator==(const Error& other) const;
  bool operator!=(const Error& other) const;
  bool ok() const { return code_ == Code::kNone; }

  Code code() const { return code_; }
  const std::string& message() const { return message_; }

  static const Error& None();

 private:
  Code code_ = Code::kNone;
  std::string message_;
};

std::ostream& operator<<(std::ostream& os, const Error::Code& code);

std::ostream& operator<<(std::ostream& out, const Error& error);

// A convenience function to return a single value from a function that can
// return a value or an error.  For normal results, construct with a ValueType*
// (ErrorOr takes ownership) and the Error will be kNone with an empty message.
// For Error results, construct with an error code and value.
//
// Example:
//
// ErrorOr<Bar> Foo::DoSomething() {
//   if (success) {
//     return Bar();
//   } else {
//     return Error(kBadThingHappened, "No can do");
//   }
// }
//
// TODO(mfoltz): Add support for type conversions.
template <typename ValueType>
class ErrorOr {
 public:
  static ErrorOr<ValueType> None() {
    static ErrorOr<ValueType> error(Error::Code::kNone);
    return error;
  }

  ErrorOr(ErrorOr&& other) = default;
  ErrorOr& operator=(ErrorOr&& other) = default;

  ErrorOr(const ValueType& value) : variant_{value} {}  // NOLINT
  ErrorOr(ValueType&& value) noexcept                   // NOLINT
      : variant_{std::move(value)} {}

  ErrorOr(Error error) : variant_{std::move(error)} {}  // NOLINT
  ErrorOr(Error::Code code) : variant_{code} {}         // NOLINT
  ErrorOr(Error::Code code, std::string message)
      : variant_{Error{code, std::move(message)}} {}

  ~ErrorOr() = default;

  bool is_error() const { return absl::holds_alternative<Error>(variant_); }
  bool is_value() const { return !is_error(); }

  // Unlike Error, we CAN provide an operator bool here, since it is
  // more obvious to callers that ErrorOr<Foo> will be true if it's Foo.
  operator bool() const { return is_value(); }

  const Error& error() const { return absl::get<Error>(variant_); }
  Error& error() { return absl::get<Error>(variant_); }

  const ValueType& value() const { return absl::get<ValueType>(variant_); }
  ValueType& value() { return absl::get<ValueType>(variant_); }

 private:
  absl::variant<Error, ValueType> variant_;
};

}  // namespace openscreen

#endif  // PLATFORM_BASE_ERROR_H_
