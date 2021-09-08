// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SETUP_H_
#define CHROME_UPDATER_SETUP_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// Installs the candidate, then posts |callback| to the main sequence. Must
// be called on the main sequence.
void InstallCandidate(UpdaterScope scope,
                      base::OnceCallback<void(int)> callback);

}  // namespace updater

#endif  // CHROME_UPDATER_SETUP_H_
