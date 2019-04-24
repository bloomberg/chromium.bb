// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"

#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/cache_storage/cache.h"

namespace blink {

namespace {

String GetDefaultMessage(mojom::CacheStorageError web_error) {
  switch (web_error) {
    case mojom::CacheStorageError::kSuccess:
      // This function should only be called with an error.
      break;
    case mojom::CacheStorageError::kErrorExists:
      return "Entry already exists.";
    case mojom::CacheStorageError::kErrorStorage:
      return "Unexpected internal error.";
    case mojom::CacheStorageError::kErrorNotFound:
      return "Entry was not found.";
    case mojom::CacheStorageError::kErrorQuotaExceeded:
      return "Quota exceeded.";
    case mojom::CacheStorageError::kErrorCacheNameNotFound:
      return "Cache was not found.";
    case mojom::CacheStorageError::kErrorQueryTooLarge:
      return "Operation too large.";
    case mojom::CacheStorageError::kErrorNotImplemented:
      return "Method is not implemented.";
    case mojom::CacheStorageError::kErrorDuplicateOperation:
      return "Duplicate operation.";
  }
  NOTREACHED();
  return String();
}

}  // namespace

DOMException* CacheStorageError::CreateException(
    mojom::CacheStorageError web_error,
    const String& message) {
  String final_message = message ? message : GetDefaultMessage(web_error);
  switch (web_error) {
    case mojom::CacheStorageError::kSuccess:
      // This function should only be called with an error.
      break;
    case mojom::CacheStorageError::kErrorExists:
      return DOMException::Create(DOMExceptionCode::kInvalidAccessError,
                                  final_message);
    case mojom::CacheStorageError::kErrorStorage:
      return DOMException::Create(DOMExceptionCode::kUnknownError,
                                  final_message);
    case mojom::CacheStorageError::kErrorNotFound:
      return DOMException::Create(DOMExceptionCode::kNotFoundError,
                                  final_message);
    case mojom::CacheStorageError::kErrorQuotaExceeded:
      return DOMException::Create(DOMExceptionCode::kQuotaExceededError,
                                  final_message);
    case mojom::CacheStorageError::kErrorCacheNameNotFound:
      return DOMException::Create(DOMExceptionCode::kNotFoundError,
                                  final_message);
    case mojom::CacheStorageError::kErrorQueryTooLarge:
      return DOMException::Create(DOMExceptionCode::kAbortError, final_message);
    case mojom::CacheStorageError::kErrorNotImplemented:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  final_message);
    case mojom::CacheStorageError::kErrorDuplicateOperation:
      return DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                  final_message);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
