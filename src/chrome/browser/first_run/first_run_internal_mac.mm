// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "chrome/browser/mac/master_prefs.h"

namespace first_run {
namespace internal {

base::FilePath MasterPrefsPath() {
  return master_prefs::MasterPrefsPath();
}

}  // namespace internal
}  // namespace first_run
