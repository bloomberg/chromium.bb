// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_LOGGING_OVERRIDE_IF_ENABLED_H_
#define MEDIA_BASE_LOGGING_OVERRIDE_IF_ENABLED_H_

// Provides a way to override DVLOGs to at build time.
// Warning: Do NOT include this file in .h files to avoid unexpected override.
// TODO(xhwang): Provide a way to choose which |verboselevel| to override.

#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_LOGGING_OVERRIDE)
#if !defined(DVLOG)
#error This file must be included after base/logging.h.
#endif
#undef DVLOG
#define DVLOG(verboselevel) LOG(INFO)
#endif  // BUILDFLAG(ENABLE_LOGGING_OVERRIDE)

#endif  // MEDIA_BASE_LOGGING_OVERRIDE_IF_ENABLED_H_
