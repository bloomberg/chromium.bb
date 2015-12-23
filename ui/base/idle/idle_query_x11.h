// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_IDLE_QUERY_X11_H_
#define UI_BASE_IDLE_IDLE_QUERY_X11_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace ui {

class IdleData;

class IdleQueryX11 {
 public:
  IdleQueryX11();
  ~IdleQueryX11();

  int IdleTime();

 private:
  scoped_ptr<IdleData> idle_data_;

  DISALLOW_COPY_AND_ASSIGN(IdleQueryX11);
};

}  // namespace ui

#endif  // UI_BASE_IDLE_IDLE_QUERY_X11_H_
