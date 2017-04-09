// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Now that all .cpp files are gone for target "wtf". However, this seems to
// trigger a compile error on Mac. This file gives an empty object file and
// should make Xcode's libtool happy...

// And also, MSVC seems to fail to build a .lib file for wtf.dll if there is
// no symbol to export.

#include "wtf/build_config.h"

#if OS(WIN)

namespace WTF {

__declspec(
    dllexport) int g_dummy_exported_value_to_force_msvc_to_generate_lib_file;
}

#endif
