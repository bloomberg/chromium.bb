// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SHOWER_APP_LIST_SHOWER_DELEGATE_FACTORY_H_
#define UI_APP_LIST_SHOWER_APP_LIST_SHOWER_DELEGATE_FACTORY_H_

#include <memory>

#include "ui/app_list/shower/app_list_shower_export.h"

namespace app_list {

class AppListShower;
class AppListShowerDelegate;

class APP_LIST_SHOWER_EXPORT AppListShowerDelegateFactory {
 public:
  virtual ~AppListShowerDelegateFactory() {}

  virtual std::unique_ptr<AppListShowerDelegate> GetDelegate(
      AppListShower* shower) = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_SHOWER_APP_LIST_SHOWER_DELEGATE_FACTORY_H_
