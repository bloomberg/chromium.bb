// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_H_
#define PRINTING_NATIVE_METAFILE_H_

#include "base/basictypes.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "printing/native_metafile_win.h"
#elif defined(OS_MACOSX)
#include "printing/native_metafile_mac.h"
#elif defined(OS_POSIX)
#include "printing/native_metafile_linux.h"
#endif

#endif  // PRINTING_NATIVE_METAFILE_H_
