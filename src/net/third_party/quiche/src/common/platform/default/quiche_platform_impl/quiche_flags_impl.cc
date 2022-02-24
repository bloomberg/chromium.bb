// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche_platform_impl/quiche_flags_impl.h"

#define QUIC_FLAG(flag, value) bool flag = value;
#include "quic/core/quic_flags_list.h"
#undef QUIC_FLAG
