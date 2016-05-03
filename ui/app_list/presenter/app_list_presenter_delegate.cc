// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/app_list_presenter_delegate.h"

#include "ui/app_list/views/app_list_view.h"

namespace app_list {
namespace {

// The minimal margin (in pixels) around the app list when in centered mode.
const int kMinimalCenteredAppListMargin = 10;

}  // namespace

int AppListPresenterDelegate::GetMinimumBoundsHeightForAppList(
    const app_list::AppListView* app_list) {
  return app_list->bounds().height() + 2 * kMinimalCenteredAppListMargin;
}

}  // namespace app_list
