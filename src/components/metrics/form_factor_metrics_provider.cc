// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/form_factor_metrics_provider.h"

#include "build/build_config.h"
#include "ui/base/device_form_factor.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace metrics {

void FormFactorMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  system_profile_proto->mutable_hardware()->set_form_factor(GetFormFactor());
}

SystemProfileProto::Hardware::FormFactor
FormFactorMetricsProvider::GetFormFactor() const {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1300338): Move the TV form factor logic to
  // ui/base/device_form_factor_android.cc.
  if (base::android::BuildInfo::GetInstance()->is_tv())
    return SystemProfileProto::Hardware::FORM_FACTOR_TV;
#endif  // BUILDFLAG(IS_ANDROID)

  switch (ui::GetDeviceFormFactor()) {
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return SystemProfileProto::Hardware::FORM_FACTOR_DESKTOP;
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return SystemProfileProto::Hardware::FORM_FACTOR_PHONE;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return SystemProfileProto::Hardware::FORM_FACTOR_TABLET;
    default:
      return SystemProfileProto::Hardware::FORM_FACTOR_UNKNOWN;
  }
}

}  // namespace metrics
