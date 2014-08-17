// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/button_drag_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/drag_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace button_drag_utils {

// Maximum width of the link drag image in pixels.
static const int kLinkDragImageMaxWidth = 150;

void SetURLAndDragImage(const GURL& url,
                        const base::string16& title,
                        const gfx::ImageSkia& icon,
                        const gfx::Point* press_pt,
                        ui::OSExchangeData* data,
                        views::Widget* widget) {
  DCHECK(url.is_valid() && data);
  data->SetURL(url, title);
  SetDragImage(url, title, icon, press_pt, data, widget);
}

void SetDragImage(const GURL& url,
                  const base::string16& title,
                  const gfx::ImageSkia& icon,
                  const gfx::Point* press_pt,
                  ui::OSExchangeData* data,
                  views::Widget* widget) {
  // Create a button to render the drag image for us.
  views::LabelButton button(NULL,
                            title.empty() ? base::UTF8ToUTF16(url.spec())
                                          : title);
  button.SetTextSubpixelRenderingEnabled(false);
  const ui::NativeTheme* theme =
      widget ? widget->GetNativeTheme() : ui::NativeTheme::instance();
  button.SetTextColor(views::Button::STATE_NORMAL,
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor));
  gfx::ShadowValues shadows(10, gfx::ShadowValue(gfx::Point(0,0), 1.0f,
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelBackgroundColor)));
  button.SetTextShadows(shadows);
  button.SetMaxSize(gfx::Size(kLinkDragImageMaxWidth, 0));
  if (icon.isNull()) {
    button.SetImage(views::Button::STATE_NORMAL,
                    *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                        IDR_DEFAULT_FAVICON).ToImageSkia());
  } else {
    button.SetImage(views::Button::STATE_NORMAL, icon);
  }
  gfx::Size prefsize = button.GetPreferredSize();
  button.SetBounds(0, 0, prefsize.width(), prefsize.height());

  gfx::Vector2d press_point;
  if (press_pt)
    press_point = press_pt->OffsetFromOrigin();
  else
    press_point = gfx::Vector2d(prefsize.width() / 2, prefsize.height() / 2);

  // Render the image.
  scoped_ptr<gfx::Canvas> canvas(
      views::GetCanvasForDragImage(widget, prefsize));
  button.Paint(canvas.get(), views::CullSet());
  drag_utils::SetDragImageOnDataObject(*canvas, press_point, data);
}

}  // namespace button_drag_utils
