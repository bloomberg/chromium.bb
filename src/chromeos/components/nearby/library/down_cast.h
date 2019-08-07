// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_DOWN_CAST_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_DOWN_CAST_H_

#include "chromeos/components/nearby/library/config.h"

#if NEARBY_USE_RTTI
#define DOWN_CAST dynamic_cast
#else
#define DOWN_CAST static_cast
#endif

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_DOWN_CAST_H_
