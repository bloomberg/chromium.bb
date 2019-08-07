// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_SCOPED_BACKLIGHTS_FORCED_OFF_H_
#define ASH_SYSTEM_POWER_SCOPED_BACKLIGHTS_FORCED_OFF_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"

namespace ash {

// RAII-style class used to force the backlights off.
// Returned by BacklightsForcedOffSetter::ForceBacklightsOff.
class ASH_EXPORT ScopedBacklightsForcedOff {
 public:
  explicit ScopedBacklightsForcedOff(base::OnceClosure unregister_callback);
  ~ScopedBacklightsForcedOff();

 private:
  // Callback that should be called in order for |this| to unregister backlights
  // forced off request.
  base::OnceClosure unregister_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBacklightsForcedOff);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_SCOPED_BACKLIGHTS_FORCED_OFF_H_
