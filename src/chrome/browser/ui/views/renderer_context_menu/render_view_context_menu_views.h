// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace views {
class Widget;
}

namespace ui {
class AcceleratorProvider;
}

class RenderViewContextMenuViews : public RenderViewContextMenu {
 public:
  ~RenderViewContextMenuViews() override;

  // Factory function to create an instance.
  static RenderViewContextMenuViews* Create(
      content::RenderFrameHost* render_frame_host,
      const content::ContextMenuParams& params);

  void RunMenuAt(views::Widget* parent,
                 const gfx::Point& point,
                 ui::MenuSourceType type);

  void ExecuteCommand(int command_id, int event_flags) override;

  // RenderViewContextMenuBase implementation.
  void Show() override;

 protected:
  RenderViewContextMenuViews(content::RenderFrameHost* render_frame_host,
                             const content::ContextMenuParams& params);

  // RenderViewContextMenu implementation.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

 private:
  void AppendPlatformEditableItems() override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // Gets the AcceleratorProvider for the browser. May be null.
  ui::AcceleratorProvider* GetBrowserAcceleratorProvider() const;

  aura::Window* GetActiveNativeView();
  views::Widget* GetTopLevelWidget();

  // Model for the BiDi input submenu.
  ui::SimpleMenuModel bidi_submenu_model_;
  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_VIEWS_H_
