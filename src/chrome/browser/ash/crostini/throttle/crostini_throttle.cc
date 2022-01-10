// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/throttle/crostini_throttle.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/concierge_helper_service.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/throttle/crostini_active_window_throttle_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace crostini {
namespace {

class DefaultDelegateImpl : public CrostiniThrottle::Delegate {
 public:
  explicit DefaultDelegateImpl(content::BrowserContext* context)
      : context_(context) {}

  DefaultDelegateImpl(const DefaultDelegateImpl&) = delete;
  DefaultDelegateImpl& operator=(const DefaultDelegateImpl&) = delete;

  ~DefaultDelegateImpl() override = default;

  void SetCpuRestriction(bool do_restrict) override {
    ash::ConciergeHelperService::GetForBrowserContext(context_)
        ->SetTerminaVmCpuRestriction(do_restrict);
  }

 private:
  content::BrowserContext* context_;
};

class CrostiniThrottleFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniThrottleFactory* GetInstance() {
    static base::NoDestructor<CrostiniThrottleFactory> instance;
    return instance.get();
  }

  static CrostiniThrottle* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<CrostiniThrottle*>(
        CrostiniThrottleFactory::GetInstance()->GetServiceForBrowserContext(
            context, true /* create */));
  }

  CrostiniThrottleFactory(const CrostiniThrottleFactory&) = delete;
  CrostiniThrottleFactory& operator=(const CrostiniThrottleFactory&) = delete;

 private:
  friend class base::NoDestructor<CrostiniThrottleFactory>;

  CrostiniThrottleFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniThrottleFactory",
            BrowserContextDependencyManager::GetInstance()) {}
  ~CrostiniThrottleFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    if (context->IsOffTheRecord())
      return nullptr;
    return new CrostiniThrottle(context);
  }
};

}  // namespace

// static
CrostiniThrottle* CrostiniThrottle::GetForBrowserContext(
    content::BrowserContext* context) {
  return CrostiniThrottleFactory::GetForBrowserContext(context);
}

CrostiniThrottle::CrostiniThrottle(content::BrowserContext* context)
    : ThrottleService(context),
      delegate_(std::make_unique<DefaultDelegateImpl>(context)) {
  AddObserver(std::make_unique<CrostiniActiveWindowThrottleObserver>());
  StartObservers();
}

CrostiniThrottle::~CrostiniThrottle() = default;

void CrostiniThrottle::Shutdown() {
  StopObservers();
}

void CrostiniThrottle::ThrottleInstance(
    ash::ThrottleObserver::PriorityLevel level) {
  switch (level) {
    case ash::ThrottleObserver::PriorityLevel::CRITICAL:
    case ash::ThrottleObserver::PriorityLevel::IMPORTANT:
    case ash::ThrottleObserver::PriorityLevel::NORMAL:
      delegate_->SetCpuRestriction(false);
      break;
    case ash::ThrottleObserver::PriorityLevel::LOW:
    case ash::ThrottleObserver::PriorityLevel::UNKNOWN:
      delegate_->SetCpuRestriction(true);
      break;
  }
}

}  // namespace crostini
