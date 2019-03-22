// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_INTERFACE_LOG_SERVICE_H_
#define CHROME_CHROME_CLEANER_LOGGING_INTERFACE_LOG_SERVICE_H_

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/logging/proto/interface_logger.pb.h"

namespace chrome_cleaner {

struct LogInformation {
  LogInformation(std::string function_name, std::string file_name);
  std::string function_name;
  std::string file_name;
};

class InterfaceLogService {
 public:
  static std::unique_ptr<InterfaceLogService> Create(
      const base::string16& file_name);

  ~InterfaceLogService();

  // Logs a call to |function_name| from the given |class_name| and also logs
  // the passed parameters recorded on |params|.
  void AddCall(const LogInformation& log_information,
               const std::map<std::string, std::string>& params);

  // Logs a call to |function_name| without parameters.
  void AddCall(const LogInformation& log_information);

  // Exposes the underlying call_record_, this is for testing purposes and
  // to provide a way to print or log the recorded calls.
  std::vector<APICall> GetCallHistory() const;

  // Returns the build version of all the logged function calls.
  std::string GetBuildVersion() const;

  base::FilePath GetLogFilePath() const;

 private:
  InterfaceLogService(const base::string16& file_name,
                      std::ofstream csv_stream);

  // TODO(joenotcharles): Currently the CallHistory is only used in the unit
  // test. Decide whether it's worth keeping.
  CallHistory call_record_;

  base::string16 log_file_name_;
  SEQUENCE_CHECKER(sequence_check_);

  // Stream to output CSV records to.
  std::ofstream csv_stream_;

  // Time at the creation of the object
  base::TimeTicks ticks_at_creation_{base::TimeTicks::Now()};

  base::TimeDelta GetTicksSinceCreation() const;

  DISALLOW_COPY_AND_ASSIGN(InterfaceLogService);
};

// Define a macro to make easier the use of AddCall.
#define CURRENT_FILE_AND_METHOD LogInformation(__func__, __FILE__)

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_INTERFACE_LOG_SERVICE_H_
