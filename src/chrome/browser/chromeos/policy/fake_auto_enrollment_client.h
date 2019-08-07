// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_FAKE_AUTO_ENROLLMENT_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_FAKE_AUTO_ENROLLMENT_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"

class PrefService;

namespace policy {

class DeviceManagementService;

// A fake AutoEnrollmentClient. The test code can control its state.
class FakeAutoEnrollmentClient : public AutoEnrollmentClient {
 public:
  // A factory that creates |FakeAutoEnrollmentClient|s.
  class FactoryImpl : public Factory {
   public:
    // The factory will notify |fake_client_created_callback| when a
    // |AutoEnrollmentClient| has been created through one of its methods.
    FactoryImpl(const base::RepeatingCallback<void(FakeAutoEnrollmentClient*)>&
                    fake_client_created_callback);
    ~FactoryImpl() override;

    std::unique_ptr<AutoEnrollmentClient> CreateForFRE(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& server_backed_state_key,
        int power_initial,
        int power_limit) override;

    std::unique_ptr<AutoEnrollmentClient> CreateForInitialEnrollment(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& device_serial_number,
        const std::string& device_brand_code,
        int power_initial,
        int power_limit,
        int power_outdated_server_detect) override;

   private:
    base::RepeatingCallback<void(FakeAutoEnrollmentClient*)>
        fake_client_created_callback_;

    DISALLOW_COPY_AND_ASSIGN(FactoryImpl);
  };

  explicit FakeAutoEnrollmentClient(const ProgressCallback& progress_callback);
  ~FakeAutoEnrollmentClient() override;

  void Start() override;
  // Note: |Retry| is currently a no-op in |FakeAutoEnrollmentClient|.
  void Retry() override;
  // Note: |CancelAndDeleteSoon| currnetly immediately deletes this
  // |FakeAutoEnrollmentClinet|.
  void CancelAndDeleteSoon() override;

  std::string device_id() const override;
  AutoEnrollmentState state() const override;

  // Sets the state and notifies the |ProgressCallback| passed to the
  // constructor.
  void SetState(AutoEnrollmentState target_state);

 private:
  ProgressCallback progress_callback_;
  AutoEnrollmentState state_;

  DISALLOW_COPY_AND_ASSIGN(FakeAutoEnrollmentClient);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_FAKE_AUTO_ENROLLMENT_CLIENT_H_
