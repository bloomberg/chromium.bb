// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SCOPED_OLE_INITIALIZER_H_
#define UI_BASE_WIN_SCOPED_OLE_INITIALIZER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/win/windows_types.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE) ScopedOleInitializer {
 public:
  ScopedOleInitializer();
  ~ScopedOleInitializer();

 private:
  THREAD_CHECKER(thread_checker_);
  HRESULT hr_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOleInitializer);
};

}  // namespace

#endif  // UI_BASE_WIN_SCOPED_OLE_INITIALIZER_H_
