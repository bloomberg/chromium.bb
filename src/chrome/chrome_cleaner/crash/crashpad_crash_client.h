// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CRASH_CRASHPAD_CRASH_CLIENT_H_
#define CHROME_CHROME_CLEANER_CRASH_CRASHPAD_CRASH_CLIENT_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/crash/crash_client.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace chrome_cleaner {

// This class manages interaction with the Crashpad reporter.
class CrashpadCrashClient : public CrashClient {
 public:
  ~CrashpadCrashClient() override;

  // Initializes the crash database only. Used in the crash reporter, which
  // cannot connect to itself to upload its own crashes.
  bool InitializeDatabaseOnly();

  crashpad::CrashReportDatabase* database() { return database_.get(); }

  // CrashClient:
  bool InitializeCrashReporting(Mode mode, SandboxType process_type) override;

  static CrashpadCrashClient* GetInstance();

  // Sets |client_id| to the current guid associated with crashes. |client_id|
  // may be empty if no guid is associated.
  static void GetClientId(base::string16* client_id);

  // Returns whether upload of crashes is enabled or not.
  static bool IsUploadEnabled();

 private:
  friend class base::Singleton<CrashpadCrashClient>;
  friend struct base::DefaultSingletonTraits<CrashpadCrashClient>;

  CrashpadCrashClient();

  // Removes already uploaded reports and limits the number of reports that
  // stay on disk.
  void DeleteStaleReports();

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<crashpad::CrashReportDatabase> database_;

  DISALLOW_COPY_AND_ASSIGN(CrashpadCrashClient);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CRASH_CRASHPAD_CRASH_CLIENT_H_
