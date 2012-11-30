// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MESA_INCLUDE_OSMESA_H_
#define THIRD_PARTY_MESA_INCLUDE_OSMESA_H_
#pragma once

// This is a shim header to include the right mesa header.
// Use this instead of referencing the mesa header directly.

#if defined(USE_SYSTEM_MESA)
#include <GL/osmesa.h>
#else
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#endif

#endif  // THIRD_PARTY_MESA_INCLUDE_OSMESA_H_
