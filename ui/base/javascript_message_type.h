// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_JAVASCRIPT_MESSAGE_TYPE_H_
#define UI_BASE_JAVASCRIPT_MESSAGE_TYPE_H_
#pragma once

#include "ui/base/ui_export.h"

namespace ui {

enum UI_EXPORT JavascriptMessageType {
  JAVASCRIPT_MESSAGE_TYPE_ALERT,
  JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
  JAVASCRIPT_MESSAGE_TYPE_PROMPT
};

}  // namespace ui

#endif  // UI_BASE_JAVASCRIPT_MESSAGE_TYPE_H_
