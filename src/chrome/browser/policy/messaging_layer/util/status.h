// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_STATUS_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_STATUS_H_

#include <cstdint>
#include <iosfwd>
#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/util/status.pb.h"

namespace reporting {
namespace error {
// These values must match error codes defined in google/rpc/code.proto
// (https://github.com/googleapis/googleapis/blob/master/google/rpc/code.proto)
enum Code : int32_t {
  OK = 0,
  CANCELLED = 1,
  UNKNOWN = 2,
  INVALID_ARGUMENT = 3,
  DEADLINE_EXCEEDED = 4,
  NOT_FOUND = 5,
  ALREADY_EXISTS = 6,
  PERMISSION_DENIED = 7,
  UNAUTHENTICATED = 16,
  RESOURCE_EXHAUSTED = 8,
  FAILED_PRECONDITION = 9,
  ABORTED = 10,
  OUT_OF_RANGE = 11,
  UNIMPLEMENTED = 12,
  INTERNAL = 13,
  UNAVAILABLE = 14,
  DATA_LOSS = 15,
};
}  // namespace error

class WARN_UNUSED_RESULT Status {
 public:
  // Creates a "successful" status.
  Status();

  // Create a status in the canonical error space with the specified
  // code, and error message.  If "code == 0", error_message is
  // ignored and a Status object identical to Status::OK is
  // constructed.
  Status(error::Code error_code, base::StringPiece error_message);
  Status(const Status&);
  Status& operator=(const Status& x);
  ~Status() = default;

  // Pre-defined Status object
  static const Status& StatusOK();

  // Accessor
  bool ok() const { return error_code_ == error::OK; }
  int error_code() const { return error_code_; }
  error::Code code() const { return error_code_; }
  base::StringPiece error_message() const { return error_message_; }
  base::StringPiece message() const { return error_message_; }

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const { return !operator==(x); }

  // Return a combination of the error code name and message.
  std::string ToString() const;

  // Exports the contents of this object into |status_proto|. This method sets
  // all fields in |status_proto| (for OK status clears |error_message|).
  void SaveTo(StatusProto* status_proto) const;

  // Populates this object using the contents of the given |status_proto|.
  void RestoreFrom(const StatusProto& status_proto);

 private:
  error::Code error_code_;
  std::string error_message_;
};

// Prints a human-readable representation of 'x' to 'os'.
std::ostream& operator<<(std::ostream& os, const Status& x);

#define CHECK_OK(value) CHECK((value).ok())
#define DCHECK_OK(value) DCHECK((value).ok())
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_STATUS_H_
