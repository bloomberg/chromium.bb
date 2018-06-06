/*
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EXCEPTION_CODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EXCEPTION_CODE_H_

#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

// DOMException's error code
// https://heycam.github.io/webidl/#idl-DOMException-error-names
enum {
  // DOMExceptions with the legacy error code.

  // The minimum value of the legacy error code of DOMException defined in
  // Web IDL.
  // https://heycam.github.io/webidl/#idl-DOMException
  kDOMExceptionLegacyCodeMin = 1,

  kIndexSizeError = 1,  // Deprecated. Use ECMAScript RangeError instead.
  // DOMStringSizeError (= 2) is deprecated and no longer supported.
  kHierarchyRequestError = 3,
  kWrongDocumentError = 4,
  kInvalidCharacterError = 5,
  // NoDataAllowedError (= 6) is deprecated and no longer supported.
  kNoModificationAllowedError = 7,
  kNotFoundError = 8,
  kNotSupportedError = 9,
  kInUseAttributeError = 10,  // Historical. Only used in setAttributeNode etc
                              // which have been removed from the DOM specs.
  kInvalidStateError = 11,
  // Web IDL 2.7.1 Error names
  // https://heycam.github.io/webidl/#idl-DOMException-error-names
  // Note: Don't confuse the "SyntaxError" DOMException defined here with
  // ECMAScript's SyntaxError. "SyntaxError" DOMException is used to report
  // parsing errors in web APIs, for example when parsing selectors, while
  // the ECMAScript SyntaxError is reserved for the ECMAScript parser.
  kSyntaxError = 12,
  kInvalidModificationError = 13,
  kNamespaceError = 14,
  // kInvalidAccessError is deprecated. Use ECMAScript TypeError for invalid
  // arguments, |kNotSupportedError| for unsupported operations, and
  // |kNotAllowedError| for denied requests instead.
  kInvalidAccessError = 15,  // Deprecated.
  // ValidationError (= 16) is deprecated and no longer supported.
  kTypeMismatchError = 17,  // Deprecated. Use ECMAScript TypeError instead.
  kSecurityError = 18,
  kNetworkError = 19,
  kAbortError = 20,
  kURLMismatchError = 21,
  kQuotaExceededError = 22,
  kTimeoutError = 23,
  kInvalidNodeTypeError = 24,
  kDataCloneError = 25,

  // The maximum value of the legacy error code of DOMException defined in
  // Web IDL.
  // https://heycam.github.io/webidl/#idl-DOMException
  kDOMExceptionLegacyCodeMax = 25,

  // DOMExceptions without the legacy error code.
  kEncodingError,
  kNotReadableError,
  kUnknownError,
  kConstraintError,
  kDataError,
  kTransactionInactiveError,
  kReadOnlyError,
  kVersionError,
  kOperationError,
  kNotAllowedError,

  // The rest of entries are defined out of scope of Web IDL.

  // DOMError (obsolete, not DOMException) defined in File system (obsolete).
  // https://www.w3.org/TR/2012/WD-file-system-api-20120417/
  kPathExistsError,

  // Push API
  //
  // PermissionDeniedError (obsolete) was replaced with NotAllowedError in the
  // standard.
  // https://github.com/WICG/BackgroundSync/issues/124
  kPermissionDeniedError,

  // Pointer Events
  // https://w3c.github.io/pointerevents/
  // Pointer Events introduced a new DOMException outside Web IDL.
  kInvalidPointerId,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EXCEPTION_CODE_H_
