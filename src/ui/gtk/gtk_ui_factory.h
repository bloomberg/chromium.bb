// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_FACTORY_H_
#define UI_GTK_GTK_UI_FACTORY_H_

#include <memory>

#include "base/component_export.h"

namespace views {
class LinuxUI;
}

// Access point to the GTK desktop system.  This should be the only symbol
// exported from this component.
COMPONENT_EXPORT(GTK)
std::unique_ptr<views::LinuxUI> BuildGtkUi();

#endif  // UI_GTK_GTK_UI_FACTORY_H_
