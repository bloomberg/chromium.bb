// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_button_linux.h"

#include <limits>

#include "base/check.h"
#include "chrome/browser/shell_integration_linux.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/wm_role_names_linux.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/transform.h"

namespace {

const int16_t kInitialWindowPos = std::numeric_limits<int16_t>::min();

class StatusIconWidget : public views::Widget {
 public:
  // xfce4-indicator-plugin requires a min size hint to be set on the window
  // (and it must be at least 2x2), otherwise it will not allocate any space to
  // the status icon window.
  gfx::Size GetMinimumSize() const override { return gfx::Size(2, 2); }
};

}  // namespace

StatusIconButtonLinux::StatusIconButtonLinux() : Button(this) {}

StatusIconButtonLinux::~StatusIconButtonLinux() = default;

void StatusIconButtonLinux::SetIcon(const gfx::ImageSkia& image) {
  SchedulePaint();
}

void StatusIconButtonLinux::SetToolTip(const base::string16& tool_tip) {
  SetTooltipText(tool_tip);
}

void StatusIconButtonLinux::UpdatePlatformContextMenu(ui::MenuModel* model) {
  // Nothing to do.
}

void StatusIconButtonLinux::OnSetDelegate() {
  widget_ = std::make_unique<StatusIconWidget>();

  const int width = std::max(1, delegate_->GetImage().width());
  const int height = std::max(1, delegate_->GetImage().height());

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.bounds =
      gfx::Rect(kInitialWindowPos, kInitialWindowPos, width, height);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.wm_role_name = ui::kStatusIconWmRoleName;
  params.wm_class_name = shell_integration_linux::GetProgramClassName();
  params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  // The status icon is a tiny window that doesn't update very often, so
  // creating a compositor would only be wasteful of resources.
  params.force_software_compositing = true;

  widget_->Init(std::move(params));

  auto* window = widget_->GetNativeWindow();
  DCHECK(window);
  host_ = window->GetHost();
  if (!host_->GetAcceleratedWidget()) {
    delegate_->OnImplInitializationFailed();
    // |this| might be destroyed.
    return;
  }

  widget_->SetContentsView(this);
  set_owned_by_client();

  SetBorder(nullptr);
  SetIcon(delegate_->GetImage());
  SetTooltipText(delegate_->GetToolTip());
  set_context_menu_controller(this);

  widget_->Show();
}

void StatusIconButtonLinux::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ui::MenuModel* menu = delegate_->GetMenuModel();
  if (!menu)
    return;
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu, views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU |
                views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(widget_.get(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void StatusIconButtonLinux::ButtonPressed(Button* sender,
                                          const ui::Event& event) {
  delegate_->OnClick();
}

void StatusIconButtonLinux::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->UndoDeviceScaleFactor();

  gfx::Rect bounds = host_->GetBoundsInPixels();
  const gfx::ImageSkia& image = delegate_->GetImage();

  // If the image fits in the window, center it.  But if it won't fit, downscale
  // it preserving aspect ratio.
  float scale =
      std::min({1.0f, static_cast<float>(bounds.width()) / image.width(),
                static_cast<float>(bounds.height()) / image.height()});
  float x_offset = (bounds.width() - scale * image.width()) / 2.0f;
  float y_offset = (bounds.height() - scale * image.height()) / 2.0f;

  gfx::Transform transform;
  transform.Translate(x_offset, y_offset);
  transform.Scale(scale, scale);
  canvas->Transform(transform);

  cc::PaintFlags flags;
  flags.setFilterQuality(kHigh_SkFilterQuality);
  canvas->DrawImageInt(image, 0, 0, image.width(), image.height(), 0, 0,
                       image.width(), image.height(), true, flags);
}