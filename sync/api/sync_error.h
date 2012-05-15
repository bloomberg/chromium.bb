// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_SYNC_ERROR_H_
#define SYNC_API_SYNC_ERROR_H_
#pragma once

#include <iosfwd>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/syncable/model_type.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

// Sync errors are used for debug purposes and handled internally and/or
// exposed through Chrome's "about:sync" internal page. They are considered
// unrecoverable for the datatype creating them, and should only be used as
// such.
// This class is copy-friendly and thread-safe.
class SyncError {
 public:
  // Default constructor refers to "no error", and IsSet() will return false.
  SyncError();

  // Create a new Sync error triggered by datatype |type| with debug message
  // |message| from the specified location. IsSet() will return true.
  // Will print the new error to LOG(ERROR).
  SyncError(const tracked_objects::Location& location,
            const std::string& message,
            syncable::ModelType type);

  // Copy and assign via deep copy.
  SyncError(const SyncError& other);
  SyncError& operator=(const SyncError& other);

  ~SyncError();

  // Reset the current error to a new error. May be called irrespective of
  // whether IsSet() is true. After this is called, IsSet() will return true.
  // Will print the new error to LOG(ERROR).
  void Reset(const tracked_objects::Location& location,
             const std::string& message,
             syncable::ModelType type);

  // Whether this is a valid error or not.
  bool IsSet() const;

  // These must only be called if IsSet() is true.
  const tracked_objects::Location& location() const;
  const std::string& message() const;
  syncable::ModelType type() const;

  // Returns empty string is IsSet() is false.
  std::string ToString() const;
 private:
  // Print error information to log.
  void PrintLogError() const;

  // Make a copy of a SyncError. If other.IsSet() == false, this->IsSet() will
  // now return false.
  void Copy(const SyncError& other);

  // Initialize the local error data with the specified error data. After this
  // is called, IsSet() will return true.
  void Init(const tracked_objects::Location& location,
            const std::string& message,
            syncable::ModelType type);

  // Reset the error to it's default (unset) values.
  void Clear();

  // scoped_ptr is necessary because Location objects aren't assignable.
  scoped_ptr<tracked_objects::Location> location_;
  std::string message_;
  syncable::ModelType type_;
};

// gmock printer helper.
void PrintTo(const SyncError& sync_error, std::ostream* os);

#endif  // SYNC_API_SYNC_ERROR_H_
