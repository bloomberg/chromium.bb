// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/updater/launchd_util.h"
#include "chrome/updater/mac/xpc_service_names.h"

namespace updater {

void AppInstall::WakeCandidateDone() {
  PollLaunchctlList(
      kUpdateLaunchdName, base::TimeDelta::FromSeconds(5),
      base::BindOnce([](scoped_refptr<AppInstall> installer,
                        bool unused) { installer->RegisterUpdater(); },
                     base::WrapRefCounted(this)));
}

}  // namespace updater
