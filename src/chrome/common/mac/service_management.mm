// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/service_management.h"

#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>

#include <errno.h>
#include <launch.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"

namespace {

class ScopedLaunchData {
 public:
  explicit ScopedLaunchData(launch_data_type_t type)
      : data_(launch_data_alloc(type)) {}
  explicit ScopedLaunchData(launch_data_t data) : data_(data) {}
  ScopedLaunchData(ScopedLaunchData&& other) : data_(other.release()) {}
  ~ScopedLaunchData() { reset(); }

  void reset() {
    if (data_)
      launch_data_free(data_);
    data_ = nullptr;
  }

  launch_data_t release() WARN_UNUSED_RESULT {
    launch_data_t val = data_;
    data_ = nullptr;
    return val;
  }

  launch_data_t get() { return data_; }
  operator launch_data_t() const { return data_; }
  operator bool() const { return !!data_; }

 private:
  launch_data_t data_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLaunchData);
};

ScopedLaunchData SendLaunchMessage(ScopedLaunchData&& msg) {
  return ScopedLaunchData(launch_msg(msg));
}

ScopedLaunchData LaunchDataFromString(const std::string& string) {
  ScopedLaunchData result(LAUNCH_DATA_STRING);
  // launch_data_set_string() will make a copy of the passed-in string.
  launch_data_set_string(result, string.c_str());
  return result;
}

int ErrnoFromLaunchData(launch_data_t data) {
  if (launch_data_get_type(data) != LAUNCH_DATA_ERRNO)
    return EINVAL;
  return launch_data_get_errno(data);
}

ScopedLaunchData DoServiceOp(const char* verb,
                             const std::string& label,
                             int* error) {
  ScopedLaunchData msg(LAUNCH_DATA_DICTIONARY);
  launch_data_dict_insert(msg, LaunchDataFromString(label).release(), verb);

  ScopedLaunchData result(SendLaunchMessage(std::move(msg)));
  if (!result)
    *error = errno;
  return result;
}

NSArray* NSArrayFromStringVector(const std::vector<std::string>& vec) {
  NSMutableArray* args = [NSMutableArray arrayWithCapacity:vec.size()];
  for (const auto& item : vec) {
    [args addObject:base::SysUTF8ToNSString(item)];
  }
  return args;
}

base::scoped_nsobject<NSDictionary> DictionaryForJobOptions(
    const mac::services::JobOptions& options) {
  base::scoped_nsobject<NSMutableDictionary> opts(
      [[NSMutableDictionary alloc] init]);

  [opts setObject:base::SysUTF8ToNSString(options.label)
           forKey:@LAUNCH_JOBKEY_LABEL];

  if (!options.executable_path.empty()) {
    [opts setObject:base::SysUTF8ToNSString(options.executable_path)
             forKey:@LAUNCH_JOBKEY_PROGRAM];
  }

  if (!options.arguments.empty()) {
    [opts setObject:NSArrayFromStringVector(options.arguments)
             forKey:@LAUNCH_JOBKEY_PROGRAMARGUMENTS];
  }

  DCHECK_EQ(options.socket_name.empty(), options.socket_key.empty());
  if (!options.socket_name.empty() && !options.socket_key.empty()) {
    NSDictionary* inner_dict = [NSDictionary
        dictionaryWithObject:base::SysUTF8ToNSString(options.socket_name)
                      forKey:@LAUNCH_JOBSOCKETKEY_PATHNAME];
    NSDictionary* outer_dict = [NSDictionary
        dictionaryWithObject:inner_dict
                      forKey:base::SysUTF8ToNSString(options.socket_key)];
    [opts setObject:outer_dict forKey:@LAUNCH_JOBKEY_SOCKETS];
  }

  if (options.run_at_load || options.auto_launch) {
    [opts setObject:@YES forKey:@LAUNCH_JOBKEY_RUNATLOAD];
  }

  if (options.auto_launch) {
    [opts setObject:@{
      @LAUNCH_JOBKEY_KEEPALIVE_SUCCESSFULEXIT : @NO
    }
             forKey:@LAUNCH_JOBKEY_KEEPALIVE];
    [opts setObject:@"Aqua" forKey:@LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE];
  }

  return base::scoped_nsobject<NSDictionary>(opts.release());
}

}  // namespace

namespace mac {
namespace services {

JobOptions::JobOptions() = default;
JobOptions::JobOptions(const JobOptions& other) = default;
JobOptions::~JobOptions() = default;

bool SubmitJob(const JobOptions& options) {
  base::scoped_nsobject<NSDictionary> options_dict =
      DictionaryForJobOptions(options);
  return SMJobSubmit(kSMDomainUserLaunchd,
                     base::mac::NSToCFCast(options_dict.get()), nullptr,
                     nullptr);
}

bool RemoveJob(const std::string& label) {
  int error = 0;
  ScopedLaunchData resp = DoServiceOp(LAUNCH_KEY_REMOVEJOB, label, &error);

  if (!error)
    error = ErrnoFromLaunchData(resp.get());

  // On macOS 10.10+, removing a running job yields EINPROGRESS but the
  // operation completes eventually (but not necessarily by the time RemoveJob
  // is done). See rdar://18398683 for details.
  if (error == EINPROGRESS)
    error = 0;

  return !error;
}

}  // namespace services
}  // namespace mac
