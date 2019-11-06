// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"

#include "base/logging.h"
#include "base/macros.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

namespace {

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

class LoginScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kBaseAttemptTimeoutSec = 5;
  static const int kMaxAttemptTimeoutSec = 30;

  explicit LoginScreenStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}
  ~LoginScreenStrategy() override = default;

 protected:
  // PortalDetectorStrategy overrides:
  StrategyId Id() const override { return STRATEGY_ID_LOGIN_SCREEN; }
  base::TimeDelta GetNextAttemptTimeoutImpl() override {
    if (DefaultNetwork() && delegate_->NoResponseResultCount() != 0) {
      int timeout = kMaxAttemptTimeoutSec;
      if (kMaxAttemptTimeoutSec / (delegate_->NoResponseResultCount() + 1) >
          kBaseAttemptTimeoutSec) {
        timeout =
            kBaseAttemptTimeoutSec * (delegate_->NoResponseResultCount() + 1);
      }
      return base::TimeDelta::FromSeconds(timeout);
    }
    return base::TimeDelta::FromSeconds(kBaseAttemptTimeoutSec);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenStrategy);
};

class ErrorScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kAttemptTimeoutSec = 15;

  explicit ErrorScreenStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}
  ~ErrorScreenStrategy() override = default;

 protected:
  // PortalDetectorStrategy overrides:
  StrategyId Id() const override { return STRATEGY_ID_ERROR_SCREEN; }
  base::TimeDelta GetNextAttemptTimeoutImpl() override {
    return base::TimeDelta::FromSeconds(kAttemptTimeoutSec);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ErrorScreenStrategy);
};

class SessionStrategy : public PortalDetectorStrategy {
 public:
  static const int kMaxFastAttempts = 3;
  static const int kFastAttemptTimeoutSec = 3;
  static const int kSlowAttemptTimeoutSec = 5;

  explicit SessionStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}
  ~SessionStrategy() override = default;

 protected:
  StrategyId Id() const override { return STRATEGY_ID_SESSION; }
  base::TimeDelta GetNextAttemptTimeoutImpl() override {
    int timeout;
    if (delegate_->NoResponseResultCount() < kMaxFastAttempts)
      timeout = kFastAttemptTimeoutSec;
    else
      timeout = kSlowAttemptTimeoutSec;
    return base::TimeDelta::FromSeconds(timeout);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStrategy);
};

}  // namespace

// PortalDetectorStrategy::Delegate --------------------------------------------

PortalDetectorStrategy::Delegate::~Delegate() = default;

// PortalDetectorStrategy -----------------------------------------------------

// static
base::TimeDelta PortalDetectorStrategy::delay_till_next_attempt_for_testing_;

// static
bool PortalDetectorStrategy::delay_till_next_attempt_for_testing_initialized_ =
    false;

// static
base::TimeDelta PortalDetectorStrategy::next_attempt_timeout_for_testing_;

// static
bool PortalDetectorStrategy::next_attempt_timeout_for_testing_initialized_ =
    false;

PortalDetectorStrategy::PortalDetectorStrategy(Delegate* delegate)
    : delegate_(delegate) {
  // First |policy_.num_errors_to_ignore| attempts with the same
  // result are performed with |policy_.initial_delay_ms| between
  // them. Delay before every consecutive attempt is multplied by
  // |policy_.multiply_factor_|. Also, |policy_.jitter_factor| is used
  // for each delay.
  policy_.num_errors_to_ignore = 3;
  policy_.initial_delay_ms = 600;
  policy_.multiply_factor = 2.0;
  policy_.jitter_factor = 0.3;
  policy_.maximum_backoff_ms = 2 * 60 * 1000;
  policy_.entry_lifetime_ms = -1;
  policy_.always_use_initial_delay = true;
  backoff_entry_.reset(new net::BackoffEntry(&policy_, delegate_));
}

PortalDetectorStrategy::~PortalDetectorStrategy() = default;

// static
std::unique_ptr<PortalDetectorStrategy> PortalDetectorStrategy::CreateById(
    StrategyId id,
    Delegate* delegate) {
  switch (id) {
    case STRATEGY_ID_LOGIN_SCREEN:
      return std::unique_ptr<PortalDetectorStrategy>(
          new LoginScreenStrategy(delegate));
    case STRATEGY_ID_ERROR_SCREEN:
      return std::unique_ptr<PortalDetectorStrategy>(
          new ErrorScreenStrategy(delegate));
    case STRATEGY_ID_SESSION:
      return std::unique_ptr<PortalDetectorStrategy>(
          new SessionStrategy(delegate));
    default:
      NOTREACHED();
      return std::unique_ptr<PortalDetectorStrategy>(
          static_cast<PortalDetectorStrategy*>(nullptr));
  }
}

base::TimeDelta PortalDetectorStrategy::GetDelayTillNextAttempt() {
  if (delay_till_next_attempt_for_testing_initialized_)
    return delay_till_next_attempt_for_testing_;
  return backoff_entry_->GetTimeUntilRelease();
}

base::TimeDelta PortalDetectorStrategy::GetNextAttemptTimeout() {
  if (next_attempt_timeout_for_testing_initialized_)
    return next_attempt_timeout_for_testing_;
  return GetNextAttemptTimeoutImpl();
}

void PortalDetectorStrategy::Reset() {
  backoff_entry_->Reset();
}

void PortalDetectorStrategy::SetPolicyAndReset(
    const net::BackoffEntry::Policy& policy) {
  policy_ = policy;
  backoff_entry_.reset(new net::BackoffEntry(&policy_, delegate_));
}

void PortalDetectorStrategy::OnDetectionCompleted() {
  backoff_entry_->InformOfRequest(false);
}

}  // namespace chromeos
