// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/platform_toast.h"

namespace vr {

PlatformToast::PlatformToast() = default;

PlatformToast::PlatformToast(base::string16 t) : text(t) {}

}  // namespace vr
