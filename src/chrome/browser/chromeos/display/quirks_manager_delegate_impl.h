// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_QUIRKS_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_QUIRKS_MANAGER_DELEGATE_IMPL_H_

#include "base/macros.h"
#include "components/quirks/quirks_manager.h"

namespace quirks {

// Working implementation of QuirksManager::Delegate for access to chrome-
// restricted parts.
class QuirksManagerDelegateImpl : public QuirksManager::Delegate {
 public:
  QuirksManagerDelegateImpl() = default;

  // QuirksManager::Delegate implementation.
  std::string GetApiKey() const override;
  base::FilePath GetDisplayProfileDirectory() const override;
  bool DevicePolicyEnabled() const override;

 private:
  ~QuirksManagerDelegateImpl() override = default;

  DISALLOW_COPY_AND_ASSIGN(QuirksManagerDelegateImpl);
};

}  // namespace quirks

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_QUIRKS_MANAGER_DELEGATE_IMPL_H_
