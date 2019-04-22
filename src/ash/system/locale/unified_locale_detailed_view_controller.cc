// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/unified_locale_detailed_view_controller.h"

#include "ash/system/locale/locale_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/logging.h"

namespace ash {

UnifiedLocaleDetailedViewController::UnifiedLocaleDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {}

UnifiedLocaleDetailedViewController::~UnifiedLocaleDetailedViewController() =
    default;

views::View* UnifiedLocaleDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::LocaleDetailedView(detailed_view_delegate_.get());
  return view_;
}

}  // namespace ash
