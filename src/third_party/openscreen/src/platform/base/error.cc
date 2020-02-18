// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/error.h"

namespace openscreen {

Error::Error() = default;

Error::Error(const Error& error) = default;

Error::Error(Error&& error) noexcept = default;

Error::Error(Code code) : code_(code) {}

Error::Error(Code code, const std::string& message)
    : code_(code), message_(message) {}

Error::Error(Code code, std::string&& message)
    : code_(code), message_(std::move(message)) {}

Error::~Error() = default;

Error& Error::operator=(const Error& other) = default;

Error& Error::operator=(Error&& other) = default;

bool Error::operator==(const Error& other) const {
  return code_ == other.code_ && message_ == other.message_;
}

std::ostream& operator<<(std::ostream& os, const Error::Code& code) {
  switch (code) {
    case Error::Code::kNone:
      return os << "Success";
    case Error::Code::kAgain:
      return os << "Transient Failure";
    case Error::Code::kCborParsing:
      return os << "Failure: CborParsing";
    case Error::Code::kCborEncoding:
      return os << "Failure: CborEncoding";
    case Error::Code::kCborIncompleteMessage:
      return os << "Failure: CborIncompleteMessage";
    case Error::Code::kCborInvalidMessage:
      return os << "Failure: CborInvalidMessage";
    case Error::Code::kCborInvalidResponseId:
      return os << "Failure: CborInvalidResponseId";
    case Error::Code::kNoAvailableReceivers:
      return os << "Failure: NoAvailableReceivers";
    case Error::Code::kRequestCancelled:
      return os << "Failure: RequestCancelled";
    case Error::Code::kNoPresentationFound:
      return os << "Failure: NoPresentationFound";
    case Error::Code::kPreviousStartInProgress:
      return os << "Failure: PreviousStartInProgress";
    case Error::Code::kUnknownStartError:
      return os << "Failure: UnknownStartError";
    case Error::Code::kUnknownRequestId:
      return os << "Failure: UnknownRequestId";
    case Error::Code::kAddressInUse:
      return os << "Failure: AddressInUse";
    case Error::Code::kDomainNameTooLong:
      return os << "Failure: DomainNameTooLong";
    case Error::Code::kDomainNameLabelTooLong:
      return os << "Failure: DomainNameLabelTooLong";
    case Error::Code::kIOFailure:
      return os << "Failure: IOFailure";
    case Error::Code::kInitializationFailure:
      return os << "Failure: InitializationFailure";
    case Error::Code::kInvalidIPV4Address:
      return os << "Failure: InvalidIPV4Address";
    case Error::Code::kInvalidIPV6Address:
      return os << "Failure: InvalidIPV6Address";
    case Error::Code::kConnectionFailed:
      return os << "Failure: ConnectionFailed";
    case Error::Code::kSocketOptionSettingFailure:
      return os << "Failure: SocketOptionSettingFailure";
    case Error::Code::kSocketBindFailure:
      return os << "Failure: SocketBindFailure";
    case Error::Code::kSocketClosedFailure:
      return os << "Failure: SocketClosedFailure";
    case Error::Code::kSocketReadFailure:
      return os << "Failure: SocketReadFailure";
    case Error::Code::kSocketSendFailure:
      return os << "Failure: SocketSendFailure";
    case Error::Code::kMdnsRegisterFailure:
      return os << "Failure: MdnsRegisterFailure";
    case Error::Code::kParseError:
      return os << "Failure: ParseError";
    case Error::Code::kUnknownMessageType:
      return os << "Failure: UnknownMessageType";
    case Error::Code::kNoActiveConnection:
      return os << "Failure: NoActiveConnection";
    case Error::Code::kAlreadyClosed:
      return os << "Failure: AlreadyClosed";
    case Error::Code::kNoStartedPresentation:
      return os << "Failure: NoStartedPresentation";
    case Error::Code::kPresentationAlreadyStarted:
      return os << "Failure: PresentationAlreadyStarted";
    case Error::Code::kInvalidConnectionState:
      return os << "Failure: InvalidConnectionState";
    case Error::Code::kJsonParseError:
      return os << "Failure: JsonParseError";
    case Error::Code::kJsonWriteError:
      return os << "Failure: JsonWriteError";
    case Error::Code::kFileLoadFailure:
      return os << "Failure: FileLoadFailure";
    case Error::Code::kErrCertsMissing:
      return os << "Failure: ErrCertsMissing";
    case Error::Code::kErrCertsParse:
      return os << "Failure: ErrCertsParse";
    case Error::Code::kErrCertsRestrictions:
      return os << "Failure: ErrCertsRestrictions";
    case Error::Code::kErrCertsDateInvalid:
      return os << "Failure: ErrCertsDateInvalid";
    case Error::Code::kErrCertsVerifyGeneric:
      return os << "Failure: ErrCertsVerifyGeneric";
    case Error::Code::kErrCrlInvalid:
      return os << "Failure: ErrCrlInvalid";
    case Error::Code::kErrCertsRevoked:
      return os << "Failure: ErrCertsRevoked";
    case Error::Code::kErrCertsPathlen:
      return os << "Failure: ErrCertsPathlen";
    case Error::Code::kUnknownError:
      return os << "Failure: UnknownError";
    case Error::Code::kNotImplemented:
      return os << "Failure: NotImplemented";
    case Error::Code::kInsufficientBuffer:
      return os << "Failure: InsufficientBuffer";
    case Error::Code::kParameterInvalid:
      return os << "Failure: ParameterInvalid";
    case Error::Code::kParameterOutOfRange:
      return os << "Failure: ParameterOutOfRange";
    case Error::Code::kParameterNullPointer:
      return os << "Failure: ParameterNullPointer";
    case Error::Code::kIndexOutOfBounds:
      return os << "Failure: IndexOutOfBounds";
    case Error::Code::kItemAlreadyExists:
      return os << "Failure: ItemAlreadyExists";
    case Error::Code::kItemNotFound:
      return os << "Failure: ItemNotFound";
    case Error::Code::kOperationInvalid:
      return os << "Failure: OperationInvalid";
    case Error::Code::kOperationCancelled:
      return os << "Failure: OperationCancelled";
  }

  // Unused 'return' to get around failure on GCC.
  return os;
}

std::ostream& operator<<(std::ostream& out, const Error& error) {
  out << error.code() << " = \"" << error.message() << "\"";
  return out;
}

// static
const Error& Error::None() {
  static Error& error = *new Error(Code::kNone);
  return error;
}

}  // namespace openscreen
