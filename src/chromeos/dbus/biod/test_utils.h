// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BIOD_TEST_UTILS_H_
#define CHROMEOS_DBUS_BIOD_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/dbus/biod/biod_client.h"

namespace dbus {
class ObjectPath;
}

namespace chromeos {
namespace test_utils {

// Copies |src_path| to |dest_path|.
void CopyObjectPath(dbus::ObjectPath* dest_path,
                    const dbus::ObjectPath& src_path);

// Copies |src_object_paths| to |dst_object_paths|.
void CopyObjectPathArray(std::vector<dbus::ObjectPath>* dest_object_paths,
                         const std::vector<dbus::ObjectPath>& src_object_paths);

// Copies |src_str| to |dest_str|.
void CopyString(std::string* dest_str, const std::string& src_str);

// Copies |src_status| to |dest_status|.
void CopyDBusMethodCallResult(bool* dest_result, bool src_result);

// Implementation of BiodClient::Observer for testing.
class TestBiodObserver : public BiodClient::Observer {
 public:
  TestBiodObserver();
  ~TestBiodObserver() override;

  int num_complete_enroll_scans_received() const {
    return num_complete_enroll_scans_received_;
  }
  int num_incomplete_enroll_scans_received() const {
    return num_incomplete_enroll_scans_received_;
  }
  int num_matched_auth_scans_received() const {
    return num_matched_auth_scans_received_;
  }
  int num_unmatched_auth_scans_received() const {
    return num_unmatched_auth_scans_received_;
  }
  int num_failures_received() const { return num_failures_received_; }
  const AuthScanMatches& last_auth_scan_matches() const {
    return last_auth_scan_matches_;
  }

  int NumEnrollScansReceived() const;
  int NumAuthScansReceived() const;

  void ResetAllCounts();

  // BiodClient::Observer:
  void BiodServiceRestarted() override;
  void BiodEnrollScanDoneReceived(biod::ScanResult scan_result,
                                  bool is_complete,
                                  int percent_complete) override;
  void BiodAuthScanDoneReceived(biod::ScanResult scan_result,
                                const AuthScanMatches& matches) override;
  void BiodSessionFailedReceived() override;

 private:
  int num_complete_enroll_scans_received_ = 0;
  int num_incomplete_enroll_scans_received_ = 0;
  int num_matched_auth_scans_received_ = 0;
  int num_unmatched_auth_scans_received_ = 0;
  int num_failures_received_ = 0;

  // When auth scan is received, store the result.
  AuthScanMatches last_auth_scan_matches_;

  DISALLOW_COPY_AND_ASSIGN(TestBiodObserver);
};

}  // namespace test_utils
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BIOD_TEST_UTILS_H_
