// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
#pragma once

#if defined(OS_WIN) && !defined(USE_AURA)
#include "ui/views/controls/table/table_view_win.h"
#else
#include "ui/views/controls/table/table_view_views.h"
#endif

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
