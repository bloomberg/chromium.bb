// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/native_library.h"

namespace gfx {

bool InitializeGLBindingsOSMesaGL();
base::NativeLibrary LoadLibrary(const char* filename);
base::NativeLibrary LoadLibrary(const base::FilePath& filename);

}  // namespace gfx
