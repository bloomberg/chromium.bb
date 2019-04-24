// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SCOPED_OLE_INITIALIZER_H_
#define UI_BASE_WIN_SCOPED_OLE_INITIALIZER_H_

#include <ole2.h>

#include "base/macros.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT ScopedOleInitializer {
 public:
  ScopedOleInitializer();
  ~ScopedOleInitializer();

 private:
#ifndef NDEBUG
  // In debug builds we use this variable to catch a potential bug where a
  // ScopedOleInitializer instance is deleted on a different thread than it
  // was initially created on.  If that ever happens it can have bad
  // consequences and the cause can be tricky to track down.
  DWORD thread_id_;
#endif
  HRESULT hr_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOleInitializer);
};

}  // namespace

#endif  // UI_BASE_WIN_SCOPED_OLE_INITIALIZER_H_
