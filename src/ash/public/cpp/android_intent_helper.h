// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ANDROID_INTENT_HELPER_H_
#define ASH_PUBLIC_CPP_ANDROID_INTENT_HELPER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"

namespace ash {

// The class which allows to launch Android intent from Ash.
class ASH_PUBLIC_EXPORT AndroidIntentHelper {
 public:
  static AndroidIntentHelper* GetInstance();

  virtual void LaunchAndroidIntent(const std::string& intent) = 0;

 protected:
  AndroidIntentHelper();
  virtual ~AndroidIntentHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidIntentHelper);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ANDROID_INTENT_HELPER_H_
