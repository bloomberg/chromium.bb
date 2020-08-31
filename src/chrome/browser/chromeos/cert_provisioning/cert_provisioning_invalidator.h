// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"

class Profile;

namespace chromeos {
namespace cert_provisioning {

enum class CertScope;

using OnInvalidationCallback = base::RepeatingClosure;

//=============== CertProvisioningInvalidationHandler ==========================

namespace internal {

// Responsible for listening to events of certificate invalidations.
// Note: An instance of invalidator will not automatically unregister given
// topic when destroyed so that subscription can be preserved if browser
// restarts. A user must explicitly call |Unregister| if subscription is not
// needed anymore.
class CertProvisioningInvalidationHandler : public syncer::InvalidationHandler {
 public:
  // Creates and registers the handler to |invalidation_service| with |topic|.
  // |on_invalidation_callback| will be called when incoming invalidation is
  // received. |scope| specifies a scope of invalidated certificate: user or
  // device.
  static std::unique_ptr<CertProvisioningInvalidationHandler> BuildAndRegister(
      CertScope scope,
      invalidation::InvalidationService* invalidation_service,
      const syncer::Topic& topic,
      OnInvalidationCallback on_invalidation_callback);

  CertProvisioningInvalidationHandler(
      CertScope scope,
      invalidation::InvalidationService* invalidation_service,
      const syncer::Topic& topic,
      OnInvalidationCallback on_invalidation_callback);
  CertProvisioningInvalidationHandler(
      const CertProvisioningInvalidationHandler&) = delete;
  CertProvisioningInvalidationHandler& operator=(
      const CertProvisioningInvalidationHandler&) = delete;

  ~CertProvisioningInvalidationHandler() override;

  // Unregisters handler and unsubscribes given topic from invalidation service.
  void Unregister();

  // syncer::InvalidationHandler:
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::TopicInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const syncer::Topic& topic) const override;

 private:
  // Registers the handler to |invalidation_service_| and subscribes with
  // |topic_|.
  // Returns true if registered successfully or if already registered,
  // false otherwise.
  bool Register();

  struct State {
    bool is_registered;
    bool is_invalidation_service_enabled;
  };

  // Represents state of current handler: whether invalidation service is
  // enabled and whether handler is registered.
  State state_{false, false};

  // Represents a handler's scope: user or device.
  const CertScope scope_;

  // An invalidation service providing the handler with incoming invalidations.
  invalidation::InvalidationService* const invalidation_service_;

  ScopedObserver<
      invalidation::InvalidationService,
      syncer::InvalidationHandler,
      &invalidation::InvalidationService::RegisterInvalidationHandler,
      &invalidation::InvalidationService::UnregisterInvalidationHandler>
      invalidation_service_observer_{this};

  // A topic representing certificate invalidations.
  const syncer::Topic topic_;

  // A callback to be called on incoming invalidation event.
  const OnInvalidationCallback on_invalidation_callback_;

  // Sequence checker to ensure that calls from invalidation service are
  // consecutive.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace internal

//=============== CertProvisioningInvalidatorFactory ===========================

class CertProvisioningInvalidator;

// Interface for a factory that creates CertProvisioningInvalidators.
class CertProvisioningInvalidatorFactory {
 public:
  CertProvisioningInvalidatorFactory() = default;
  CertProvisioningInvalidatorFactory(
      const CertProvisioningInvalidatorFactory&) = delete;
  CertProvisioningInvalidatorFactory& operator=(
      const CertProvisioningInvalidatorFactory&) = delete;
  virtual ~CertProvisioningInvalidatorFactory() = default;

  virtual std::unique_ptr<CertProvisioningInvalidator> Create() = 0;
};

//=============== CertProvisioningInvalidator ==================================

// An invalidator that calls a Callback when an invalidation for a specific
// topic has been received. Register can be called multiple times for the same
// topic (e.g. after a chrome restart).
class CertProvisioningInvalidator {
 public:
  CertProvisioningInvalidator();
  CertProvisioningInvalidator(const CertProvisioningInvalidator&) = delete;
  CertProvisioningInvalidator& operator=(const CertProvisioningInvalidator&) =
      delete;
  virtual ~CertProvisioningInvalidator();

  virtual void Register(const syncer::Topic& topic,
                        OnInvalidationCallback on_invalidation_callback) = 0;
  virtual void Unregister();

 protected:
  std::unique_ptr<internal::CertProvisioningInvalidationHandler>
      invalidation_handler_;
};

//=============== CertProvisioningUserInvalidatorFactory =======================

// This factory creates CertProvisioningInvalidators that use the passed user
// Profile's InvalidationService.
class CertProvisioningUserInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  explicit CertProvisioningUserInvalidatorFactory(Profile* profile);
  std::unique_ptr<CertProvisioningInvalidator> Create() override;

 private:
  Profile* profile_ = nullptr;
};

//=============== CertProvisioningUserInvalidator ==============================

class CertProvisioningUserInvalidator : public CertProvisioningInvalidator {
 public:
  explicit CertProvisioningUserInvalidator(Profile* profile);

  void Register(const syncer::Topic& topic,
                OnInvalidationCallback on_invalidation_callback) override;

 private:
  Profile* profile_ = nullptr;
};

//=============== CertProvisioningDeviceInvalidatorFactory =====================

// This factory creates CertProvisioningInvalidators that use the device-wide
// InvalidationService.
class CertProvisioningDeviceInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  explicit CertProvisioningDeviceInvalidatorFactory(
      policy::AffiliatedInvalidationServiceProvider* service_provider);
  std::unique_ptr<CertProvisioningInvalidator> Create() override;

 private:
  policy::AffiliatedInvalidationServiceProvider* service_provider_ = nullptr;
};

//=============== CertProvisioningDeviceInvalidator ============================

class CertProvisioningDeviceInvalidator
    : public CertProvisioningInvalidator,
      public policy::AffiliatedInvalidationServiceProvider::Consumer {
 public:
  explicit CertProvisioningDeviceInvalidator(
      policy::AffiliatedInvalidationServiceProvider* service_provider);
  ~CertProvisioningDeviceInvalidator() override;

  void Register(const syncer::Topic& topic,
                OnInvalidationCallback on_invalidation_callback) override;
  void Unregister() override;

 private:
  // policy::AffiliatedInvalidationServiceProvider::Consumer
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

  syncer::Topic topic_;
  OnInvalidationCallback on_invalidation_callback_;
  policy::AffiliatedInvalidationServiceProvider* service_provider_ = nullptr;
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_INVALIDATOR_H_
