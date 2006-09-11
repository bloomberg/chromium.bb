// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// CrashReport encapsulates all of the information for a single report,
// including data sent with the report and data derived by the minidump
// processor (stack trace, etc.).

#ifndef GOOGLE_CRASH_REPORT_H__
#define GOOGLE_CRASH_REPORT_H__

#include <vector>
#include <string>
#include "google/airbag_types.h"
#include "google/stack_frame.h"

namespace google_airbag {

using std::vector;
using std::string;

struct CrashReport {
  // An optional id for the report.  This is supplied by the caller
  // and is not used by airbag.
  string report_id;

  // An optional id which identifies the client that generated the report.
  string client_id;

  // The time that the report was uploaded (milliseconds since the epoch)
  airbag_time_t report_time;

  // The uptime of the process before it crashed
  airbag_time_t process_uptime;

  // The cumulative uptime of the application for this user
  airbag_time_t cumulative_product_uptime;

  // The user's email address, if they provided it
  string user_email;

  // Extra comments the user provided, e.g. steps to reproduce the crash
  string user_comments;

  // The product which sent this report
  struct Product {
    string name;
    string version;
  };
  Product product;

  // The user's operating system
  struct OS {
    string name;
    string version;
  };
  OS os;

  // The user's CPU architecture (x86, PowerPC, x86_64, ...)
  string cpu_arch;

  // Extra product-specific key/value pairs
  struct ProductData {
    string key;
    string value;
  };
  vector<ProductData> product_data;

  // The stack frames for the crashed thread, top-first.m
  // TODO(bryner): accommodate multiple threads
  StackFrames stack_frames;

  // The stack trace "signature", which can be used for matching up reports
  string stack_signature;
};

}  // namespace google_airbag

#endif  // GOOGLE_CRASH_REPORT_H__
