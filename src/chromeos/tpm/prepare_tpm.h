// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TPM_PREPARE_TPM_H_
#define CHROMEOS_TPM_PREPARE_TPM_H_

#include "base/callback.h"
#include "base/component_export.h"

namespace chromeos {

// Asynchronously prepares TPM. To be specific, attempts to clear owner
// password if TPM is owned to make sure the owner password is cleared if no
// longer needed; otherwise if TPM is not owned, which can happen due to
// interrupted TPM initialization triggered upon showing EULA screen, triggers
// TPM initialization process. When the preparation process is done, invoke
// `preparation_finished_callback`.
void COMPONENT_EXPORT(CHROMEOS_TPM)
    PrepareTpm(base::OnceClosure preparation_finished_callback);

}  // namespace chromeos

#endif  // CHROMEOS_TPM_PREPARE_TPM_H_
