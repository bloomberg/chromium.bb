// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SEARCH_RESULT_OBSERVER_H_
#define UI_APP_LIST_SEARCH_RESULT_OBSERVER_H_
#pragma once

#include "ui/app_list/app_list_export.h"

namespace app_list {

class APP_LIST_EXPORT SearchResultObserver {
 public:
  // Invoked when SearchResult icon has changed.
  virtual void OnIconChanged() = 0;

 protected:
  virtual ~SearchResultObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_SEARCH_RESULT_OBSERVER_H_
