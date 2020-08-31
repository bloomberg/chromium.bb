// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_NFC_NFC_SYSTEM_LEVEL_SETTING_IMPL_H_
#define COMPONENTS_PERMISSIONS_ANDROID_NFC_NFC_SYSTEM_LEVEL_SETTING_IMPL_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/permissions/android/nfc/nfc_system_level_setting.h"

namespace permissions {

class NfcSystemLevelSettingImpl : public NfcSystemLevelSetting {
 public:
  NfcSystemLevelSettingImpl();
  ~NfcSystemLevelSettingImpl() override;

  // NfcSystemLevelSetting implementation:
  bool IsNfcAccessPossible() override;
  bool IsNfcSystemLevelSettingEnabled() override;
  void PromptToEnableNfcSystemLevelSetting(
      content::WebContents* web_contents,
      base::OnceClosure prompt_completed_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcSystemLevelSettingImpl);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_NFC_NFC_SYSTEM_LEVEL_SETTING_IMPL_H_
