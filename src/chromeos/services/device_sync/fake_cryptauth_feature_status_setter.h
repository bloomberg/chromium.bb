// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_SETTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_SETTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/services/device_sync/cryptauth_feature_status_setter.h"
#include "chromeos/services/device_sync/cryptauth_feature_status_setter_impl.h"
#include "chromeos/services/device_sync/network_request_error.h"

namespace chromeos {

namespace device_sync {

class CryptAuthClientFactory;

class FakeCryptAuthFeatureStatusSetter : public CryptAuthFeatureStatusSetter {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSetFeatureStatusCalled() {}
  };

  struct Request {
    Request(const std::string& device_id,
            multidevice::SoftwareFeature feature,
            FeatureStatusChange status_change,
            base::OnceClosure success_callback,
            base::OnceCallback<void(NetworkRequestError)> error_callback);

    Request(Request&& request);

    ~Request();

    const std::string device_id;
    const multidevice::SoftwareFeature feature;
    const FeatureStatusChange status_change;
    base::OnceClosure success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
  };

  FakeCryptAuthFeatureStatusSetter();

  FakeCryptAuthFeatureStatusSetter(const FakeCryptAuthFeatureStatusSetter&) =
      delete;
  FakeCryptAuthFeatureStatusSetter& operator=(
      const FakeCryptAuthFeatureStatusSetter&) = delete;

  ~FakeCryptAuthFeatureStatusSetter() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Request>& requests() { return requests_; }

 private:
  // CryptAuthFeatureStatusSetter:
  void SetFeatureStatus(
      const std::string& device_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;

  Delegate* delegate_ = nullptr;
  std::vector<Request> requests_;
};

class FakeCryptAuthFeatureStatusSetterFactory
    : public CryptAuthFeatureStatusSetterImpl::Factory {
 public:
  FakeCryptAuthFeatureStatusSetterFactory();

  FakeCryptAuthFeatureStatusSetterFactory(
      const FakeCryptAuthFeatureStatusSetterFactory&) = delete;
  FakeCryptAuthFeatureStatusSetterFactory& operator=(
      const FakeCryptAuthFeatureStatusSetterFactory&) = delete;

  ~FakeCryptAuthFeatureStatusSetterFactory() override;

  const std::vector<FakeCryptAuthFeatureStatusSetter*>& instances() const {
    return instances_;
  }

  const std::string& last_instance_id() const { return last_instance_id_; }

  const std::string& last_instance_id_token() const {
    return last_instance_id_token_;
  }

  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

 private:
  // CryptAuthFeatureStatusSetterImpl::Factory:
  std::unique_ptr<CryptAuthFeatureStatusSetter> CreateInstance(
      const std::string& instance_id,
      const std::string& instance_id_token,
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer) override;

  std::vector<FakeCryptAuthFeatureStatusSetter*> instances_;
  std::string last_instance_id_;
  std::string last_instance_id_token_;
  CryptAuthClientFactory* last_client_factory_ = nullptr;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_SETTER_H_
