// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/scoped_ole_initializer.h"

#include <ole2.h>

#include "base/logging.h"

namespace ui {

ScopedOleInitializer::ScopedOleInitializer() {
  HRESULT result = OleInitialize(NULL);
  DCHECK(SUCCEEDED(result));
}

ScopedOleInitializer::~ScopedOleInitializer() {
  OleUninitialize();
}

}  // namespace ui
