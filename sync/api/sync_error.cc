// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_error.h"

#include <ostream>

#include "base/location.h"
#include "base/logging.h"
#include "sync/syncable/model_type.h"

SyncError::SyncError() {
  Clear();
}

SyncError::SyncError(const tracked_objects::Location& location,
                     const std::string& message,
                     syncable::ModelType type) {
  Init(location, message, type);
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
         other.type());
  } else {
    Clear();
  }
}

void SyncError::Clear() {
  location_.reset();
  message_ = std::string();
  type_ = syncable::UNSPECIFIED;
}

void SyncError::Reset(const tracked_objects::Location& location,
                      const std::string& message,
                      syncable::ModelType type) {
  Init(location, message, type);
  PrintLogError();
}

void SyncError::Init(const tracked_objects::Location& location,
                     const std::string& message,
                     syncable::ModelType type) {
  location_.reset(new tracked_objects::Location(location));
  message_ = message;
  type_ = type;
}

bool SyncError::IsSet() const {
  return location_.get() != NULL;
}


const tracked_objects::Location& SyncError::location() const {
  CHECK(IsSet());
  return *location_;
}

const std::string& SyncError::message() const {
  CHECK(IsSet());
  return message_;
}

syncable::ModelType SyncError::type() const {
  CHECK(IsSet());
  return type_;
}

std::string SyncError::ToString() const {
  if (!IsSet()) {
    return std::string();
  }
  return location_->ToString() + ", " + syncable::ModelTypeToString(type_) +
      ", Sync Error: " + message_;
}

void SyncError::PrintLogError() const {
  LAZY_STREAM(logging::LogMessage(location_->file_name(),
                                  location_->line_number(),
                                  logging::LOG_ERROR).stream(),
              LOG_IS_ON(ERROR))
      << syncable::ModelTypeToString(type_) << ", Sync Error: " << message_;
}

void PrintTo(const SyncError& sync_error, std::ostream* os) {
  *os << sync_error.ToString();
}
