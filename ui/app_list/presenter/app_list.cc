// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/app_list.h"

namespace app_list {

AppList::AppList() {}

AppList::~AppList() {}

void AppList::BindRequest(mojom::AppListRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

mojom::AppListPresenter* AppList::GetAppListPresenter() {
  return presenter_.get();
}

void AppList::SetAppListPresenter(mojom::AppListPresenterPtr presenter) {
  presenter_ = std::move(presenter);
}

}  // namespace app_list
