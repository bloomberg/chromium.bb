// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_INVALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_INVALIDATOR_H_

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_invalidator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace cert_provisioning {

class MockCertProvisioningInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  MockCertProvisioningInvalidatorFactory();
  virtual ~MockCertProvisioningInvalidatorFactory();

  MOCK_METHOD(std::unique_ptr<CertProvisioningInvalidator>,
              Create,
              (),
              (override));

  void ExpectCreateReturnNull();
};

class MockCertProvisioningInvalidator : public CertProvisioningInvalidator {
 public:
  MockCertProvisioningInvalidator();
  virtual ~MockCertProvisioningInvalidator();

  MOCK_METHOD(void,
              Register,
              (const syncer::Topic& topic,
               OnInvalidationCallback on_invalidation_callback),
              (override));

  MOCK_METHOD(void, Unregister, (), (override));
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_INVALIDATOR_H_
