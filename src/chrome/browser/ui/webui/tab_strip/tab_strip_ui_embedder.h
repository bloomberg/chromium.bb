// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_EMBEDDER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_EMBEDDER_H_

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/point.h"

// Interface to be implemented by the embedder. Provides native UI
// functionality such as showing context menus.
class TabStripUIEmbedder {
 public:
  TabStripUIEmbedder() = default;
  virtual ~TabStripUIEmbedder() = default;

  virtual const ui::AcceleratorProvider* GetAcceleratorProvider() const = 0;

  virtual void CloseContainer() = 0;

  virtual void ShowContextMenuAtPoint(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) = 0;

  virtual void ShowEditDialogForGroupAtPoint(gfx::Point point,
                                             gfx::Rect rect,
                                             tab_groups::TabGroupId group) = 0;

  virtual TabStripUILayout GetLayout() = 0;

  virtual SkColor GetColor(int id) const = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_EMBEDDER_H_
