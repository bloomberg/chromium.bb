// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_error.h"

#include <ostream>

#include "base/location.h"
#include "base/logging.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

SyncError::SyncError() {
  Clear();
}

SyncError::SyncError(const tracked_objects::Location& location,
                     ErrorType error_type,
                     const std::string& custom_message,
                     ModelType model_type) {
  std::string type_message;
  switch (error_type) {
    case UNRECOVERABLE_ERROR:
      type_message = "unrecoverable error was encountered: ";
      break;
    case DATATYPE_ERROR:
      type_message = "datatype error was encountered: ";
      break;
    case PERSISTENCE_ERROR:
      type_message = "persistence error was encountered: ";
      break;
    case CRYPTO_ERROR:
      type_message = "cryptographer error was encountered: ";
      break;
    default:
      NOTREACHED();
      type_message = "invalid error: ";
  }
  Init(location, type_message + custom_message, model_type, error_type);
  PrintLogError();
}

SyncError::SyncError(const SyncError& other) {
  Copy(other);
}

SyncError::~SyncError() {
}

SyncError& SyncError::operator=(const SyncError& other) {
  if (this == &other) {
    return *this;
  }
  Copy(other);
  return *this;
}

void SyncError::Copy(const SyncError& other) {
  if (other.IsSet()) {
    Init(other.location(),
         other.message(),
         other.model_type(),
         other.error_type());
  } else {
    Clear();
  }
}

void SyncError::Clear() {
  location_.reset();
  message_ = std::string();
  model_type_ = UNSPECIFIED;
  error_type_ = UNSET;
}

void SyncError::Reset(const tracked_objects::Location& location,
                      const std::string& message,
                      ModelType model_type) {
  Init(location, message, model_type, DATATYPE_ERROR);
  PrintLogError();
}

void SyncError::Init(const tracked_objects::Location& location,
                     const std::string& message,
                     ModelType model_type,
                     ErrorType error_type) {
  location_.reset(new tracked_objects::Location(location));
  message_ = message;
  model_type_ = model_type;
  error_type_ = error_type;
}

bool SyncError::IsSet() const {
  return error_type_ != UNSET;
}


const tracked_objects::Location& SyncError::location() const {
  CHECK(IsSet());
  return *location_;
}

const std::string& SyncError::message() const {
  CHECK(IsSet());
  return message_;
}

ModelType SyncError::model_type() const {
  CHECK(IsSet());
  return model_type_;
}

SyncError::ErrorType SyncError::error_type() const {
  CHECK(IsSet());
  return error_type_;
}

std::string SyncError::ToString() const {
  if (!IsSet()) {
    return std::string();
  }
  return location_->ToString() + ", " + ModelTypeToString(model_type_) +
      " " + message_;
}

void SyncError::PrintLogError() const {
  LAZY_STREAM(logging::LogMessage(location_->file_name(),
                                  location_->line_number(),
                                  logging::LOG_ERROR).stream(),
              LOG_IS_ON(ERROR))
      << ModelTypeToString(model_type_) << " " << message_;
}

void PrintTo(const SyncError& sync_error, std::ostream* os) {
  *os << sync_error.ToString();
}

}  // namespace syncer
