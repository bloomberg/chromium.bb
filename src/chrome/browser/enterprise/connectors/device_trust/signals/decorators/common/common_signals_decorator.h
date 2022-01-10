// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_

#include <string>

#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace enterprise_connectors {

// Definition of the SignalsDecorator common to all platforms.
class CommonSignalsDecorator : public SignalsDecorator {
 public:
  CommonSignalsDecorator(PrefService* local_state, PrefService* profile_prefs);
  ~CommonSignalsDecorator() override;

  // SignalsDecorator:
  void Decorate(SignalsType& signals, base::OnceClosure done_closure) override;

 private:
  void OnHardwareInfoRetrieved(SignalsType& signals,
                               base::TimeTicks start_time,
                               base::OnceClosure done_closure,
                               base::SysInfo::HardwareInfo hardware_info);

  void UpdateFromCache(SignalsType& signals);

  PrefService* local_state_;
  PrefService* profile_prefs_;

  // These two signals are fetched asynchronously and their collection can
  // involve expensive operations such as reading from disk. Since these signals
  // are not expected to change throughout the browser's lifetime, they will be
  // cached in this decorator.
  absl::optional<std::string> cached_device_model_;
  absl::optional<std::string> cached_device_manufacturer_;

  base::WeakPtrFactory<CommonSignalsDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DECORATORS_COMMON_COMMON_SIGNALS_DECORATOR_H_
