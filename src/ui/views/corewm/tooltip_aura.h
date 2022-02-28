// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_AURA_H_
#define UI_VIEWS_COREWM_TOOLTIP_AURA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class RenderText;
class Size;
}  // namespace gfx

namespace ui {
struct AXNodeData;
struct OwnedWindowAnchor;
}  // namespace ui

namespace views {

class Widget;

namespace corewm {
namespace test {
class TooltipAuraTestApi;
}

// Implementation of Tooltip that shows the tooltip using a Widget and Label.
class VIEWS_EXPORT TooltipAura : public Tooltip, public WidgetObserver {
 public:
  static const char kWidgetName[];
  // FIXME: get cursor offset from actual cursor size.
  static constexpr int kCursorOffsetX = 10;
  static constexpr int kCursorOffsetY = 15;

  TooltipAura() = default;

  TooltipAura(const TooltipAura&) = delete;
  TooltipAura& operator=(const TooltipAura&) = delete;

  ~TooltipAura() override;

 private:
  class TooltipWidget;

  friend class test::TooltipAuraTestApi;
  gfx::RenderText* GetRenderTextForTest();
  void GetAccessibleNodeDataForTest(ui::AXNodeData* node_data);

  // Adjusts the bounds given by the arguments to fit inside the desktop
  // and returns the adjusted bounds, and also sets anchor information to
  // |anchor|.
  gfx::Rect GetTooltipBounds(const gfx::Size& tooltip_size,
                             const TooltipPosition& position,
                             ui::OwnedWindowAnchor* anchor);

  // Sets |widget_| to a new instance of TooltipWidget. Additional information
  // that helps to position anchored windows in such backends as Wayland.
  void CreateTooltipWidget(const gfx::Rect& bounds,
                           const ui::OwnedWindowAnchor& anchor);

  // Destroys |widget_|.
  void DestroyWidget();

  // Tooltip:
  int GetMaxWidth(const gfx::Point& location) const override;
  void Update(aura::Window* window,
              const std::u16string& tooltip_text,
              const TooltipPosition& position) override;
  void Show() override;
  void Hide() override;
  bool IsVisible() override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  // The widget containing the tooltip. May be NULL.
  raw_ptr<TooltipWidget> widget_ = nullptr;

  // The window we're showing the tooltip for. Never NULL and valid while
  // showing.
  raw_ptr<aura::Window> tooltip_window_ = nullptr;
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_AURA_H_
