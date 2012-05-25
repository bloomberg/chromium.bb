// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"

namespace app_list {

AppListModel::AppListModel()
    : apps_(new Apps),
      search_box_(new SearchBoxModel),
      results_(new SearchResults) {
}

AppListModel::~AppListModel() {
}

}  // namespace app_list
